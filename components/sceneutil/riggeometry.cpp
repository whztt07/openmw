#include "riggeometry.hpp"

#include <stdexcept>
#include <iostream>

#include <cstdlib>

#include <osg/MatrixTransform>
#include <osg/io_utils>

#include "skeleton.hpp"
#include "bone.hpp"

namespace
{

Eigen::Vector4f toEigen(const osg::Vec3f& vec)
{
    Eigen::Vector4f result;
    result << vec.x(), vec.y(), vec.z(), 1.f;
    return result;
}

// Eigen transformation instead of matrix4?
void transformBoundingSphere (const Eigen::Matrix4f& matrix, osg::BoundingSphere& bsphere)
{
    Eigen::Vector4f xdash = toEigen(bsphere._center);
    xdash(0) += bsphere._radius;
    xdash = matrix * xdash;

    Eigen::Vector4f ydash = toEigen(bsphere._center);
    ydash(1) += bsphere._radius;
    ydash = matrix * ydash;

    Eigen::Vector4f zdash = toEigen(bsphere._center);
    zdash(2) += bsphere._radius;
    zdash = matrix * zdash;

    Eigen::Vector4f centerEigen;
    centerEigen << bsphere._center.x(), bsphere._center.y(), bsphere._center.z(), 1.f;
    centerEigen = matrix * centerEigen;

    bsphere._center = osg::Vec3f(centerEigen(0), centerEigen(1), centerEigen(2));

    // TODO: don't need to get length of all 3?
    xdash -= centerEigen;
    float sqrlen_xdash = xdash.squaredNorm();

    ydash -= centerEigen;
    float sqrlen_ydash = ydash.squaredNorm();

    zdash -= centerEigen;
    float sqrlen_zdash = zdash.squaredNorm();

    bsphere._radius = sqrlen_xdash;
    if (bsphere._radius<sqrlen_ydash) bsphere._radius = sqrlen_ydash;
    if (bsphere._radius<sqrlen_zdash) bsphere._radius = sqrlen_zdash;
    bsphere._radius = sqrtf(bsphere._radius);
}

}

namespace SceneUtil
{

class UpdateRigBounds : public osg::Drawable::UpdateCallback
{
public:
    UpdateRigBounds()
    {
    }

    UpdateRigBounds(const UpdateRigBounds& copy, const osg::CopyOp& copyop)
        : osg::Drawable::UpdateCallback(copy, copyop)
    {
    }

    void update(osg::NodeVisitor* nv, osg::Drawable* drw)
    {
        RigGeometry* rig = static_cast<RigGeometry*>(drw);

        rig->updateBounds(nv);
    }
};

// TODO: make threadsafe for multiple cull threads
class UpdateRigGeometry : public osg::Drawable::CullCallback
{
public:
    UpdateRigGeometry()
    {
    }

    UpdateRigGeometry(const UpdateRigGeometry& copy, const osg::CopyOp& copyop)
        : osg::Drawable::CullCallback(copy, copyop)
    {
    }

    META_Object(NifOsg, UpdateRigGeometry)

    virtual bool cull(osg::NodeVisitor* nv, osg::Drawable* drw, osg::State*) const
    {
        RigGeometry* geom = static_cast<RigGeometry*>(drw);
        geom->update(nv);
        return false;
    }
};

RigGeometry::RigGeometry()
    : mFirstFrame(true)
    , mBoundsFirstFrame(true)
{
    setCullCallback(new UpdateRigGeometry);
    setUpdateCallback(new UpdateRigBounds);
    setSupportsDisplayList(false);
}

RigGeometry::RigGeometry(const RigGeometry &copy, const osg::CopyOp &copyop)
    : osg::Geometry(copy, copyop)
    , mInfluenceMap(copy.mInfluenceMap)
    , mFirstFrame(copy.mFirstFrame)
    , mBoundsFirstFrame(copy.mBoundsFirstFrame)
{
    setSourceGeometry(copy.mSourceGeometry);
}

void RigGeometry::setSourceGeometry(osg::ref_ptr<osg::Geometry> sourceGeometry)
{
    mSourceGeometry = sourceGeometry;

    osg::Geometry& from = *sourceGeometry;

    if (from.getStateSet())
        setStateSet(from.getStateSet());

    // copy over primitive sets.
    getPrimitiveSetList() = from.getPrimitiveSetList();

    if (from.getColorArray())
        setColorArray(from.getColorArray());

    if (from.getSecondaryColorArray())
        setSecondaryColorArray(from.getSecondaryColorArray());

    if (from.getFogCoordArray())
        setFogCoordArray(from.getFogCoordArray());

    for(unsigned int ti=0;ti<from.getNumTexCoordArrays();++ti)
    {
        if (from.getTexCoordArray(ti))
            setTexCoordArray(ti,from.getTexCoordArray(ti));
    }

    osg::Geometry::ArrayList& arrayList = from.getVertexAttribArrayList();
    for(unsigned int vi=0;vi< arrayList.size();++vi)
    {
        osg::Array* array = arrayList[vi].get();
        if (array)
            setVertexAttribArray(vi,array);
    }

    setVertexArray(dynamic_cast<osg::Array*>(from.getVertexArray()->clone(osg::CopyOp::DEEP_COPY_ALL)));
    setNormalArray(dynamic_cast<osg::Array*>(from.getNormalArray()->clone(osg::CopyOp::DEEP_COPY_ALL)), osg::Array::BIND_PER_VERTEX);
}

bool RigGeometry::initFromParentSkeleton(osg::NodeVisitor* nv)
{
    const osg::NodePath& path = nv->getNodePath();
    for (osg::NodePath::const_reverse_iterator it = path.rbegin(); it != path.rend(); ++it)
    {
        osg::Node* node = *it;
        if (Skeleton* skel = dynamic_cast<Skeleton*>(node))
        {
            mSkeleton = skel;
            break;
        }
    }

    if (!mSkeleton)
    {
        std::cerr << "A RigGeometry did not find its parent skeleton" << std::endl;
        return false;
    }

    if (!mInfluenceMap)
    {
        std::cerr << "No InfluenceMap set on RigGeometry" << std::endl;
        return false;
    }

    typedef std::map<unsigned short, std::vector<BoneWeight> > Vertex2BoneMap;
    Vertex2BoneMap vertex2BoneMap;
    for (std::map<std::string, BoneInfluence>::const_iterator it = mInfluenceMap->mMap.begin(); it != mInfluenceMap->mMap.end(); ++it)
    {
        Bone* bone = mSkeleton->getBone(it->first);
        if (!bone)
        {
            std::cerr << "RigGeometry did not find bone " << it->first << std::endl;
            continue;
        }

        mBoneSphereMap[bone] = it->second.mBoundSphere;

        const BoneInfluence& bi = it->second;

        const std::map<unsigned short, float>& weights = it->second.mWeights;
        for (std::map<unsigned short, float>::const_iterator weightIt = weights.begin(); weightIt != weights.end(); ++weightIt)
        {
            std::vector<BoneWeight>& vec = vertex2BoneMap[weightIt->first];

            BoneWeight b = std::make_pair(std::make_pair(bone, bi.mInvBindMatrix), weightIt->second);

            vec.push_back(b);
        }
    }

    for (Vertex2BoneMap::iterator it = vertex2BoneMap.begin(); it != vertex2BoneMap.end(); it++)
    {
        mBone2VertexMap[it->second].push_back(it->first);
    }

    return true;
}

void accummulateMatrix(const Eigen::Matrix4f& invBindMatrix, const Eigen::Matrix4f& matrix, float weight, Eigen::Matrix4f& result)
{
    result += (matrix * invBindMatrix) * weight;
}

void RigGeometry::update(osg::NodeVisitor* nv)
{
    if (!mSkeleton)
    {
        if (!initFromParentSkeleton(nv))
            return;
    }

    if (!mSkeleton->getActive() && !mFirstFrame)
        return;
    mFirstFrame = false;

    mSkeleton->updateBoneMatrices(nv);

    osg::Matrixf geomToSkel = getGeomToSkelMatrix(nv);
    Eigen::Matrix4f geomToSkelEigen;
    for (int i=0; i<4; ++i)
        for (int j=0; j<4; ++j)
            geomToSkelEigen(j,i) = geomToSkel(i,j);

    // skinning
    osg::Vec3Array* positionSrc = static_cast<osg::Vec3Array*>(mSourceGeometry->getVertexArray());
    osg::Vec3Array* normalSrc = static_cast<osg::Vec3Array*>(mSourceGeometry->getNormalArray());

    osg::Vec3Array* positionDst = static_cast<osg::Vec3Array*>(getVertexArray());
    osg::Vec3Array* normalDst = static_cast<osg::Vec3Array*>(getNormalArray());

    for (Bone2VertexMap::const_iterator it = mBone2VertexMap.begin(); it != mBone2VertexMap.end(); ++it)
    {
        Eigen::Matrix4f resultMat;
        resultMat << 0, 0, 0, 0,
                     0, 0, 0, 0,
                     0, 0, 0, 0,
                     0, 0, 0, 1;

        for (std::vector<BoneWeight>::const_iterator weightIt = it->first.begin(); weightIt != it->first.end(); ++weightIt)
        {
            Bone* bone = weightIt->first.first;
            const osg::Matrix& invBindMatrix = weightIt->first.second;
            Eigen::Matrix4f invBindMatrixEigen;
            for (int i=0; i<4; ++i)
                for (int j=0; j<4; ++j)
                    invBindMatrixEigen(j,i) = invBindMatrix(i,j);

            float weight = weightIt->second;
            const Eigen::Matrix4f& boneMatrix = bone->mMatrixInSkeletonSpace;
            accummulateMatrix(invBindMatrixEigen, boneMatrix, weight, resultMat);
        }
        resultMat = geomToSkelEigen * resultMat;

        for (std::vector<unsigned short>::const_iterator vertexIt = it->second.begin(); vertexIt != it->second.end(); ++vertexIt)
        {
            unsigned short vertex = *vertexIt;
            Eigen::Vector4f positionVec;
            positionVec << (*positionSrc)[vertex].x(),
                        (*positionSrc)[vertex].y(),
                        (*positionSrc)[vertex].z(),
                            1.f;

            Eigen::Vector3f normalVec;
            normalVec << (*normalSrc)[vertex].x(),
                        (*normalSrc)[vertex].y(),
                        (*normalSrc)[vertex].z();

            positionVec = resultMat * positionVec;
            normalVec = resultMat.block<3,3>(0,0) * normalVec;

            (*positionDst)[vertex].x() = positionVec(0);
            (*positionDst)[vertex].y() = positionVec(1);
            (*positionDst)[vertex].z() = positionVec(2);

            (*normalDst)[vertex].x() = normalVec(0);
            (*normalDst)[vertex].y() = normalVec(1);
            (*normalDst)[vertex].z() = normalVec(2);
        }
    }

    positionDst->dirty();
    normalDst->dirty();
}

void RigGeometry::updateBounds(osg::NodeVisitor *nv)
{
    if (!mSkeleton)
    {
        if (!initFromParentSkeleton(nv))
            return;
    }

    if (!mSkeleton->getActive() && !mBoundsFirstFrame)
        return;
    mBoundsFirstFrame = false;

    mSkeleton->updateBoneMatrices(nv);

    osg::Matrixf geomToSkel = getGeomToSkelMatrix(nv);
    Eigen::Matrix4f geomToSkelEigen;
    for (int i=0; i<4; ++i)
        for (int j=0; j<4; ++j)
            geomToSkelEigen(j,i) = geomToSkel(i,j);

    osg::BoundingBox box;
    for (BoneSphereMap::const_iterator it = mBoneSphereMap.begin(); it != mBoneSphereMap.end(); ++it)
    {
        Bone* bone = it->first;
        osg::BoundingSpheref bs = it->second;
        transformBoundingSphere(geomToSkelEigen * bone->mMatrixInSkeletonSpace, bs);
        box.expandBy(bs);
    }

    _boundingBox = box;
    for (unsigned int i=0; i<getNumParents(); ++i)
        getParent(i)->dirtyBound();
}

osg::Matrixf RigGeometry::getGeomToSkelMatrix(osg::NodeVisitor *nv)
{
    osg::NodePath path;
    bool foundSkel = false;
    for (osg::NodePath::const_iterator it = nv->getNodePath().begin(); it != nv->getNodePath().end(); ++it)
    {
        if (!foundSkel)
        {
            if (*it == mSkeleton)
                foundSkel = true;
        }
        else
            path.push_back(*it);
    }
    return osg::computeWorldToLocal(path);

}

void RigGeometry::setInfluenceMap(osg::ref_ptr<InfluenceMap> influenceMap)
{
    mInfluenceMap = influenceMap;
}


}

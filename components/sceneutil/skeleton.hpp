#ifndef OPENMW_COMPONENTS_NIFOSG_SKELETON_H
#define OPENMW_COMPONENTS_NIFOSG_SKELETON_H

#include <osg/Group>

#include <memory>

/** \addtogroup Skinning
 *  @{
 */

namespace SceneUtil
{

    class Bone;

    /// @brief Handles the bone matrices for any number of child RigGeometries.
    /// @par Bones should be created as osg::MatrixTransform children of the skeleton.
    /// To be a referenced by a RigGeometry, a bone needs to have a unique name.
    class Skeleton : public osg::Group
    {
    public:
        Skeleton();
        Skeleton(const Skeleton& copy, const osg::CopyOp& copyop);

        META_Node(NifOsg, Skeleton)

        /// Retrieve a bone by name.
        Bone* getBone(const std::string& name);

        /// Request an update of bone matrices. May be a no-op if already updated in this frame.
        void updateBoneMatrices(osg::NodeVisitor* nv);

        /// Set the skinning active flag. Inactive skeletons will not have their child rigs updated.
        /// You should set this flag to false if you know that bones are not currently moving.
        void setActive(bool active);

        bool getActive() const;

    private:
        // The root bone is not a "real" bone, it has no corresponding node in the scene graph.
        // As far as the scene graph goes we support multiple root bones.
        std::auto_ptr<Bone> mRootBone;

        typedef std::map<std::string, std::pair<osg::NodePath, osg::MatrixTransform*> > BoneCache;
        BoneCache mBoneCache;
        bool mBoneCacheInit;

        bool mNeedToUpdateBoneMatrices;

        bool mActive;

        unsigned int mLastFrameNumber;
    };

}

/** @} */

#endif

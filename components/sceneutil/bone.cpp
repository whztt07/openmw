#include "bone.hpp"

#include <iostream>

#include <osg/MatrixTransform>

namespace SceneUtil
{

    Bone::Bone()
        : mNode(NULL)
    {
    }

    Bone::~Bone()
    {
        for (unsigned int i=0; i<mChildren.size(); ++i)
            delete mChildren[i];
        mChildren.clear();
    }

    void Bone::update(const Eigen::Matrix4f* parentMatrixInSkeletonSpace)
    {
        if (!mNode)
        {
            std::cerr << "Bone without node " << std::endl;
        }

        Eigen::Matrix4f nodeMatrix;
        for (int i=0; i<4; ++i)
            for (int j=0; j<4; ++j)
                nodeMatrix(j,i) = mNode->getMatrix()(i,j);

        if (parentMatrixInSkeletonSpace)
            mMatrixInSkeletonSpace = (*parentMatrixInSkeletonSpace) * nodeMatrix;
        else
            mMatrixInSkeletonSpace = nodeMatrix;

        for (unsigned int i=0; i<mChildren.size(); ++i)
        {
            mChildren[i]->update(&mMatrixInSkeletonSpace);
        }
    }

}

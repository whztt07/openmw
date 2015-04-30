#ifndef OPENMW_COMPONENTS_SCENEUTIL_BONE_H
#define OPENMW_COMPONENTS_SCENEUTIL_BONE_H

#include <Eigen/Dense>
#include <vector>

namespace osg
{
    class MatrixTransform;
}

/** \addtogroup Skinning
 *  @{
 */

namespace SceneUtil
{

/// @brief Defines a Bone hierarchy, used for updating of skeleton-space bone matrices.
/// @note To prevent unnecessary updates, only bones that are used for skinning will be added to this hierarchy.
class Bone
{
public:
    Bone();
    ~Bone();

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    Eigen::Matrix4f mMatrixInSkeletonSpace;

    osg::MatrixTransform* mNode;

    std::vector<Bone*> mChildren;

    /// Update the skeleton-space matrix of this bone and all its children.
    void update(const Eigen::Matrix4f* parentMatrixInSkeletonSpace);

private:
    Bone(const Bone&);
    void operator=(const Bone&);
};

}

/** @} */

#endif

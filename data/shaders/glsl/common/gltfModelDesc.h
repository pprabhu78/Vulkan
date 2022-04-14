#if CPU_SIDE_COMPILATION
using namespace glm;
namespace genesis
{
#else
#endif

struct ModelDesc
{
   uint64_t textureOffset;
   uint64_t vertexBufferAddress;
   uint64_t indexBufferAddress;
   uint64_t indexIndicesAddress;
   uint64_t materialAddress;       // Address of the material buffer
   uint64_t materialIndicesAddress;       // Address of the material buffer
};

#if CPU_SIDE_COMPILATION
}
#else
#endif


#include "component/RenderComponent.hpp"
#include "server/Server.hpp"
#include "RayTracingRenderer.hpp"

using namespace std;
using namespace NRenderer;

namespace RayTracing
{
    // 继承RenderComponent, 复写render接口
    class Adapter : public RenderComponent
    {
        void render(SharedScene spScene) {
            RayTracingRenderer rayTrace{ spScene };
            auto result = rayTrace.render();
            auto [pixels, width, height] = result;
            getServer().screen.set(pixels, width, height);
            rayTrace.release(result);
        }
    };
}

// REGISTER_RENDERER(Name, Description, Class)
REGISTER_RENDERER(RayTracing, "ray tracing", RayTracing::Adapter);
#pragma once
#ifndef __RAY_CAST_HPP__
#define __RAY_CAST_HPP__

#include "scene/Scene.hpp"

#include "Camera.hpp"
#include "intersections/intersections.hpp"
#include "KdTree.hpp"
#include "shaders/ShaderCreator.hpp"

namespace RayTracing
{
    using namespace NRenderer;
    class RayTracingRenderer
    {
    private:
        SharedScene spScene;
        Scene& scene;
        RayTracing::Camera camera;
        KDTree kd_tree;
        vector<SharedShader> shaderPrograms;
    public:
        RayTracingRenderer(SharedScene spScene)
            : spScene(spScene)
            , scene(*spScene)
            , camera(spScene->camera)
        {}
        ~RayTracingRenderer() = default;

        using RenderResult = tuple<RGBA*, unsigned int, unsigned int>;
        RenderResult render();
        void release(const RenderResult& r);

    private:
        RGB gamma(const RGB& rgb);
        RGB trace(const Ray& r, int count);
        HitRecord closestHit(const Ray& r);
    };
}

#endif
#include "RayTracingRenderer.hpp"

#include "VertexTransformer.hpp"
#include "intersections/intersections.hpp"
namespace RayTracing
{
	void RayTracingRenderer::release(const RenderResult& r) {
		auto [p, w, h] = r;
		delete[] p;
	}
	RGB RayTracingRenderer::gamma(const RGB& rgb) {
		return glm::sqrt(rgb);
	}
	auto RayTracingRenderer::render() -> RenderResult {
		auto width = scene.renderOption.width;
		auto height = scene.renderOption.height;
		auto pixels = new RGBA[width * height];

		VertexTransformer vertexTransformer{};
		vertexTransformer.exec(spScene);

		ShaderCreator shaderCreator{};
		for (auto& mtl : scene.materials) {
			shaderPrograms.push_back(shaderCreator.create(mtl, scene.textures));
		}

		kd_tree.insert(scene.sphereBuffer, scene.triangleBuffer);

		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				auto ray = camera.shoot(float(j) / float(width), float(i) / float(height));
				auto color = trace(ray, 0);
				color = clamp(color);
				color = gamma(color);
				pixels[(height - i - 1) * width + j] = { color, 1 };
			}
		}

		return { pixels, width, height };
	}

	RGB RayTracingRenderer::trace(const Ray& r, int count) {
		// 达反射次数上限
		if (scene.pointLightBuffer.size() < 1 || count >= 50) 
			return { 0, 0, 0 };
		auto& l = scene.pointLightBuffer[0];
		auto closestHitObj = closestHit(r);
		if (closestHitObj) {
			auto& hitRec = *closestHitObj;
			//向量标准化   
			auto out = glm::normalize(l.position - hitRec.hitPoint);
			auto distance = glm::length(l.position - hitRec.hitPoint);
			auto shadowRay = Ray{ hitRec.hitPoint, out };
			auto shadowHit = closestHit(shadowRay);
			auto c = shaderPrograms[hitRec.material.index()]->shade(-r.direction, out, hitRec.normal);
			// 光线与反射点法向夹角
			float inc_angle = abs(glm::dot(hitRec.normal, r.direction));
			// 反射光线
			auto out_ray = Ray{ hitRec.hitPoint, glm::normalize(r.direction + 2 * inc_angle * hitRec.normal) };
			auto c_new = shaderPrograms[hitRec.material.index()]
				->shade(-r.direction, glm::normalize(r.direction + 2 * inc_angle * hitRec.normal), hitRec.normal);

			if ((!shadowHit) || (shadowHit && shadowHit->t > distance)) {
				//递归追踪
				if (scene.materials[hitRec.material.index()].type == 1 || scene.materials[hitRec.material.index()].type == 3)
					return c * l.intensity + trace(out_ray, count + 1) * c_new;			
				else
					return c * l.intensity;
			}
			else {
				//递归追踪
				if (scene.materials[hitRec.material.index()].type == 1 || scene.materials[hitRec.material.index()].type == 3)
					return Vec3{ 0 } + trace(out_ray, count + 1) * c_new;
				else
					return Vec3{ 0 };
			}
		}
		else {
			return { 0, 0, 0 };
		}
	}

	HitRecord RayTracingRenderer::closestHit(const Ray& r) {
		HitRecord closestHit = nullopt;
		float closest = FLOAT_INF;

		//KdTree
		for (auto& n : kd_tree.find_node(r)) {
			for (auto& b : n->block_list) {
				HitRecord hitRecord;
				if (b->type == SPHERE) 
					hitRecord = Intersection::xSphere(r, *(b->sphere), 0.01, closest);
				else if (b->type == TRIANGLE) 
					hitRecord = Intersection::xTriangle(r, *(b->triangle), 0.01, closest);
				if (hitRecord && hitRecord->t < closest) {
					closest = hitRecord->t;
					closestHit = hitRecord;
				}
			}
		}
		
		for (auto& p : scene.planeBuffer) {
			auto hitRecord = Intersection::xPlane(r, p, 0.01, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		return closestHit;
	}
}
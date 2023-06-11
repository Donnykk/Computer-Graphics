#include "server/Server.hpp"

#include "PhotonMapping.hpp"

#include "VertexTransformer.hpp"
#include "intersections/intersections.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "Onb.hpp"  


//unfinished
namespace PhotonMapping
{
	RGB PhotonMappingRenderer::gamma(const RGB& rgb) {
		return glm::sqrt(rgb);
	}
	int index = 0;

	void PhotonMappingRenderer::renderTask(RGBA* pixels, int width, int height) {
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				Vec3 color{ 0, 0, 0 };
				for (int k = 0; k < samples; k++) {
					auto r = defaultSamplerInstance<UniformInSquare>().sample2d();
					float rx = r.x;
					float ry = r.y;
					float x = (float(j) + rx) / float(width);
					float y = (float(i) + ry) / float(height);
					auto ray = camera.shoot(x, y);
					color += trace(ray, 0);
				}
				color /= samples;
				color = gamma(color);
				pixels[(height - i - 1) * width + j] = { color, 1 };
			}
		}
	}

	auto PhotonMappingRenderer::render() -> RenderResult {
		// shaders
		shaderPrograms.clear();
		ShaderCreator shaderCreator{};
		for (auto& m : scene.materials) {
			shaderPrograms.push_back(shaderCreator.create(m, scene.textures));
		}

		RGBA* pixels = new RGBA[width * height]{};

		// 局部坐标转换成世界坐标
		VertexTransformer vertexTransformer{};
		vertexTransformer.exec(spScene);
		for (int i = 0; i < scene.areaLightBuffer.size(); i++) {
			auto length_u = glm::length(scene.areaLightBuffer[i].u);
			auto length_v = glm::length(scene.areaLightBuffer[i].v);
			auto norm_u = glm::normalize(scene.areaLightBuffer[i].u);
			auto norm_v = glm::normalize(scene.areaLightBuffer[i].v);
			auto sin_0 = std::pow(1 - std::pow(glm::dot(norm_u, norm_v), 2), 0.5);
			float A = length_u * length_v * sin_0;
			auto x = norm_u.y * norm_v.z - norm_v.y * norm_u.z;
			auto y = -(norm_u.x * norm_v.z - norm_v.x * norm_u.z);
			auto z = norm_u.x * norm_v.y - norm_v.x * norm_u.y;
			auto n =  Vec3{ x,y,z };
			scene.areaLightBuffer[i].A = A;
			scene.areaLightBuffer[i].n = n;
		}
		//构建光子映射
		for (int i = 0; i < num_photon; i++) {
			for (int j = 0; j < scene.areaLightBuffer.size(); j++) {
				Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
				Onb onb{ scene.areaLightBuffer[j].n };
				Vec3 direction = glm::normalize(onb.local(random));
				Vec2 random_ = defaultSamplerInstance<UniformInSquare>().sample2d();
				auto origin = scene.areaLightBuffer[j].position
					+ random_.x * scene.areaLightBuffer[j].u
					+ random_.y * scene.areaLightBuffer[j].v;
				Ray ray{ origin, direction };

				Vec3 r = (scene.areaLightBuffer[j].radiance / ((1.f / scene.areaLightBuffer[j].A) * PI));
				photonMap(ray, r, 0);
			}
		}
		PhotonMappingRenderer::renderTask(pixels, width, height);
		getServer().logger.log("Done...");
		return { pixels, width, height };
	}

	void PhotonMappingRenderer::release(const RenderResult& r) {
		auto [p, w, h] = r;
		delete[] p;
	}

	HitRecord PhotonMappingRenderer::closestHitObject(const Ray& r) {
		HitRecord closestHit = nullopt;
		float closest = FLOAT_INF;
		for (auto& s : scene.sphereBuffer) {
			auto hitRecord = Intersection::xSphere(r, s, 0.01, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		for (auto& t : scene.triangleBuffer) {
			auto hitRecord = Intersection::xTriangle(r, t, 0.01, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		for (auto& p : scene.planeBuffer) {
			auto hitRecord = Intersection::xPlane(r, p, 0.000001, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		return closestHit;
	}

	tuple<float, Vec3> PhotonMappingRenderer::closestHitLight(const Ray& r) {
		Vec3 v = {};
		HitRecord closest = getHitRecord(FLOAT_INF, {}, {}, {});
		for (auto& a : scene.areaLightBuffer) {
			auto hitRecord = Intersection::xAreaLight(r, a, 0.000001, closest->t);
			if (hitRecord && closest->t > hitRecord->t) {
				closest = hitRecord;
				v = a.radiance;
			}
		}
		return { closest->t, v };
	}


	void PhotonMappingRenderer::photonMap(Ray& ray, Vec3 r, int currDepth) {
		auto hitObject = closestHitObject(ray);
		if (!hitObject)
			return;
		else {
			auto mtlHandle = hitObject->material;
			auto scattered = shaderPrograms[mtlHandle.index()]->shade(ray, hitObject->hitPoint, hitObject->normal);
			auto attenuation = scattered.attenuation;
			auto emitted = scattered.emitted;
			Ray next = scattered.ray;
			//漫反射
			if (scene.materials[mtlHandle.index()].type == 0) {
				photon p{ hitObject->hitPoint, r, hitObject->normal, ray, next };
				photons.push_back(p);
				if (currDepth >= depth || rand() % 5 < 1)
					return;
				photonMap(next, attenuation * r * abs(glm::dot(hitObject->normal, ray.direction)) / (scattered.pdf * 0.8f), currDepth + 1);
			}
			else if (scene.materials[mtlHandle.index()].type == 3) {
				if (currDepth >= depth || rand() % 5 < 1)
					return;
				photonMap(next, attenuation * r / (scattered.pdf * 0.8f), currDepth + 1);
				//折射
				next = scattered.refraction_ray;
				photonMap(next, scattered.refraction * r / (scattered.pdf * 0.8f), currDepth + 1);
			}
		}
	}

	RGB PhotonMappingRenderer::trace(const Ray& r, int currDepth) {
		auto hitObject = closestHitObject(r);
		auto [t, emitted] = closestHitLight(r);
		// hit object
		if (hitObject && hitObject->t < t) {
			auto mtlHandle = hitObject->material;
			auto scattered = shaderPrograms[mtlHandle.index()]->shade(r, hitObject->hitPoint, hitObject->normal);
			auto scatteredRay = scattered.ray;
			auto attenuation = scattered.attenuation;
			auto emitted = scattered.emitted;

			if (scene.materials[mtlHandle.index()].hasProperty("ior")) {
				RGB next = Vec3(0.f);
				if (currDepth > depth) 
					next = (attenuation + scattered.refraction) * scene.ambient.constant;
				else {
					auto reflex = trace(scattered.ray, currDepth + 1);
					RGB refraction = Vec3(0.f);
					if (scattered.refraction_ray.direction != Vec3(0.f)) refraction = trace(scattered.refraction_ray, currDepth + 1);
					next = attenuation * reflex + refraction * scattered.refraction;
				}
				return emitted + (next / scattered.pdf) / 0.8f;
			}

			Vec3 dir{ 0,0,0 };
			Vec3 r{ 0,0,0 };

			float max_r = 0.f;
			float n_dot_in = glm::dot(hitObject->normal, glm::normalize(dir));
			float pdf = scattered.pdf;

			auto Lc = (attenuation * r * n_dot_in / pdf);
			return emitted + Lc;
		}
		else if (t != FLOAT_INF) {
			return emitted;
		}
		else {
			return Vec3{ 0 };
		}
	}
}
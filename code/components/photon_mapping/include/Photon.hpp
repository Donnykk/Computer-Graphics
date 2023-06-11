#ifndef PHOTON_H
#define PHOTON_H

#include "scene/Scene.hpp"
#include "Ray.hpp"
#include "PhotonMapping.hpp"

struct photon {
    PhotonMapping::Vec3 position;
    PhotonMapping::Vec3 r;
    PhotonMapping::Vec3 norm;
    PhotonMapping::Ray in, out;
    float operator[](int index) const {
        return position[index];
    }
    float& operator[](int index) {
        return position[index];
    }
};

#endif
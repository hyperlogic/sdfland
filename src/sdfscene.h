//
//  SDFScene.h
//
//  Created by Anthony J. Thibault on 9/2/15.
//  Copyright (c) 2019 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SDFScene_h
#define hifi_SDFScene_h

#include <vector>
#include <glm/glm.hpp>

struct Prim;

class SDFScene {
public:
    SDFScene();
    ~SDFScene();

    int GetSize() const { return _size; }
    const float* GetBuffer() const { return _buffer; }

    void AddCircle(const glm::vec2& pos, float radius);
    void RemCircle(const glm::vec2& pos, float radius);

    int GetSamplesPerMeter() const;

    int _size;
    float* _buffer;
    std::vector<Prim> _prims;
};

#endif

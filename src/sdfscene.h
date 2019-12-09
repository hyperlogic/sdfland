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

struct Prim;

class SDFScene {
public:
    SDFScene();
    ~SDFScene();

    int GetWidth() const { return _width; }
    int GetHeight() const { return _height; }
    const uint8_t* GetTextureBuffer() const { return _textureBuffer; }
    const float* GetBuffer() const { return _buffer; }

    int _width;
    int _height;
    float* _buffer;
    uint8_t* _textureBuffer;
    std::vector<Prim> _prims;
};

#endif

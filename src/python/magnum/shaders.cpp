/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h> /* for vector arguments */
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Shaders/VertexColor.h>

#include "Corrade/Python.h"

#include "corrade/EnumOperators.h"
#include "magnum/bootstrap.h"

namespace magnum {

namespace {

template<UnsignedInt dimensions> void vertexColor(PyNonDestructibleClass<Shaders::VertexColor<dimensions>, GL::AbstractShaderProgram>& c) {
    /* Attributes */
    c.attr("COLOR3") = GL::DynamicAttribute{typename Shaders::VertexColor<dimensions>::Color3{}};
    c.attr("COLOR4") = GL::DynamicAttribute{typename Shaders::VertexColor<dimensions>::Color4{}};

    /* Methods */
    c
        .def(py::init(), "Constructor")

        /* Using lambdas to avoid method chaining getting into signatures */

        .def_property("transformation_projection_matrix", nullptr, &Shaders::VertexColor<dimensions>::setTransformationProjectionMatrix,
            "Transformation and projection matrix");
}

}

void shaders(py::module& m) {
    m.doc() = "Builtin shaders";

    #ifndef MAGNUM_BUILD_STATIC
    /* These are a part of the same module in the static build, no need to
       import (also can't import because there it's _magnum.*) */
    py::module::import("magnum.gl");
    #endif

    /* 2D/3D vertex color shader */
    {
        PyNonDestructibleClass<Shaders::VertexColor2D, GL::AbstractShaderProgram> vertexColor2D{m,
            "VertexColor2D", "2D vertex color shader"};
        PyNonDestructibleClass<Shaders::VertexColor3D, GL::AbstractShaderProgram> vertexColor3D{m,
            "VertexColor3D", "3D vertex color shader"};
        vertexColor2D.attr("POSITION") = GL::DynamicAttribute{Shaders::VertexColor2D::Position{}};
        vertexColor3D.attr("POSITION") = GL::DynamicAttribute{Shaders::VertexColor3D::Position{}};
        vertexColor(vertexColor2D);
        vertexColor(vertexColor3D);
    }

    /* Phong shader */
    {
        PyNonDestructibleClass<Shaders::Phong, GL::AbstractShaderProgram> phong{m,
            "Phong", "Phong shader"};
        phong.attr("POSITION") = GL::DynamicAttribute{Shaders::Phong::Position{}};
        phong.attr("NORMAL") = GL::DynamicAttribute{Shaders::Phong::Normal{}};
        phong.attr("TANGENT") = GL::DynamicAttribute{Shaders::Phong::Tangent{}};
        phong.attr("TEXTURE_COORDINATES") = GL::DynamicAttribute{Shaders::Phong::TextureCoordinates{}};
        phong.attr("COLOR3") = GL::DynamicAttribute{Shaders::Phong::Color3{}};
        phong.attr("COLOR4") = GL::DynamicAttribute{Shaders::Phong::Color4{}};

        py::enum_<Shaders::Phong::Flag> flags{phong, "Flags", "Flags"};

        flags
            .value("AMBIENT_TEXTURE", Shaders::Phong::Flag::AmbientTexture)
            .value("DIFFUSE_TEXTURE", Shaders::Phong::Flag::DiffuseTexture)
            .value("SPECULAR_TEXTURE", Shaders::Phong::Flag::SpecularTexture)
            .value("NORMAL_TEXTURE", Shaders::Phong::Flag::NormalTexture)
            .value("ALPHA_MASK", Shaders::Phong::Flag::AlphaMask)
            .value("VERTEX_COLOR", Shaders::Phong::Flag::VertexColor)
            .value("NONE", Shaders::Phong::Flag{})
            /* TODO: OBJECT_ID, once multiple FB outputs and mapDraw is exposed */
            ;
        corrade::enumOperators(flags);

        phong
            .def(py::init<Shaders::Phong::Flag, UnsignedInt>(), "Constructor",
                py::arg("flags") = Shaders::Phong::Flag{},
                py::arg("light_count") = 1)
            .def_property_readonly("flags", [](Shaders::Phong& self) {
                return Shaders::Phong::Flag(UnsignedByte(self.flags()));
            }, "Flags")
            .def_property_readonly("light_count", &Shaders::Phong::lightCount,
                "Light count")
            .def_property("ambient_color", nullptr,
                &Shaders::Phong::setAmbientColor, "Ambient color")
            .def_property("diffuse_color", nullptr,
                &Shaders::Phong::setDiffuseColor, "Diffuse color")
            .def_property("specular_color", nullptr,
                &Shaders::Phong::setSpecularColor, "Specular color")
            // TODO: textures, once exposed
            .def_property("shininess", nullptr,
                &Shaders::Phong::setShininess, "Shininess")
            .def_property("alpha_mask", nullptr, [](Shaders::Phong& self, Float mask) {
                if(!(self.flags() & Shaders::Phong::Flag::AlphaMask)) {
                    PyErr_SetString(PyExc_AttributeError, "the shader was not created with alpha mask enabled");
                    throw py::error_already_set{};
                }

                self.setAlphaMask(mask);
            }, "Alpha mask")
            .def_property("transformation_matrix", nullptr,
                &Shaders::Phong::setTransformationMatrix, "Set transformation matrix")
            .def_property("normal_matrix", nullptr,
                &Shaders::Phong::setNormalMatrix, "Set normal matrix")
            .def_property("projection_matrix", nullptr,
                &Shaders::Phong::setProjectionMatrix, "Set projection matrix")
            .def_property("light_positions", nullptr, [](Shaders::Phong& self, const std::vector<Vector3>& positions) {
                if(positions.size() != self.lightCount()) {
                    PyErr_Format(PyExc_ValueError, "expected %u items but got %u", self.lightCount(), UnsignedInt(positions.size()));
                    throw py::error_already_set{};
                }

                self.setLightPositions(positions);
            }, "Light positions")
            .def_property("light_colors", nullptr, [](Shaders::Phong& self, const std::vector<Color4>& colors) {
                if(colors.size() != self.lightCount()) {
                    PyErr_Format(PyExc_ValueError, "expected %u items but got %u", self.lightCount(), UnsignedInt(colors.size()));
                    throw py::error_already_set{};
                }

                self.setLightColors(colors);
            }, "Light colors");
    }
}

}

#ifndef MAGNUM_BUILD_STATIC
/* TODO: remove declaration when https://github.com/pybind/pybind11/pull/1863
   is released */
extern "C" PYBIND11_EXPORT PyObject* PyInit_shaders();
PYBIND11_MODULE(shaders, m) {
    magnum::shaders(m);
}
#endif

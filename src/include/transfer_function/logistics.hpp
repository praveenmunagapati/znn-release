//
// Copyright (C) 2012-2015  Aleksandar Zlateski <zlateski@mit.edu>
// ---------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#pragma once

#include "../options/options.hpp"

#include <type_traits>
#include <cmath>
#include <string>

namespace znn { namespace v4 { namespace functions {

struct logistics
{
    real operator()(real x) const noexcept
    {
        return static_cast<real>(1) / (static_cast<real>(1) + std::exp(-x));
    }

    real grad(real f) const noexcept
    {
        return f * (static_cast<real>(1) - f);
    }


    options serialize() const
    {
        options ret;
        ret.push("function", "logistics");
        return ret;
    }


};


struct forward_logistics
{
    real operator()(real x) const noexcept
    {
        return static_cast<real>(1) / (static_cast<real>(1) + std::exp(-x));
    }


    options serialize() const
    {
        options ret;
        ret.push("function", "forward_logistics");
        return ret;
    }

};

}}} // namespace znn::v4::functions

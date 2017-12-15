/*****************************************************************************
 * Copyright (C) 2013 x265 project
 *
 * Authors: Selvakumar Nithiyaruban <selvakumar@multicorewareinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#ifndef X265_FILTERS_H
#define X265_FILTERS_H
 
#include "x265.h"
#include "common.h"
#include "cstring"
#include "cstdio"
#include <vector>

using namespace std;
 
namespace X265_NS {
class Filter
{
public:
    virtual ~Filter() {};
    static bool parseFilterString(char* paramString, vector<Filter *>* filters);
    Filter() {}
    virtual void setParam(x265_param*) = 0;
    virtual bool isFail() const = 0;
    virtual void release() = 0;
    virtual void processFrame(x265_picture&) = 0;
};
}

#endif //X265_FILTERS_H

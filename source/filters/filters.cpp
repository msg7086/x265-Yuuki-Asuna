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

#include "filters.h"
#include "common.h"
#ifdef ENABLE_ZIMG
#include "zimgfilter.h"
#endif

using namespace X265_NS;

bool Filter::parseFilterString(char* paramString, vector<Filter *>* filters)
{
    // --vf func1:param1/func2:param2
    char* end = paramString + strlen(paramString);
    char* begin = paramString;
    char* p = begin;
    while(p < end)
    {
        char fName[1024];
        char fParams[1024];
        int length;
        // Scan to find column sign
        while(p[0] != ':' && p[0] != '/' && p < end) p++;
        length = p - begin;
        fName[length] = 0;
        strncpy(fName, begin, length);
        p = begin = p + 1;

        while(p[0] != '/' && p < end) p++;
        length = p - begin;
        fParams[length] = 0;
        strncpy(fParams, begin, length);
        p = begin = p + 1;

        if (fName[0])
        {
            Filter* filter = NULL;
#ifdef ENABLE_ZIMG
            if (!strcmp(fName, "zimg"))
                filter = new ZimgFilter(fParams);
#endif
            if (filter == NULL)
            {
                x265_log(NULL, X265_LOG_ERROR, "Unknown filter: %s\n", fName);
                return true;
            }
            filters->push_back(filter);
            if (filter->isFail())
                return true;
        }
    }
    return false;
}

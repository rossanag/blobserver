/*
 * Copyright (C) 2012 Emmanuel Durand
 *
 * This file is part of blobserver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * The blob base class.
 */

#include "opencv2/opencv.hpp"

class Blob
{
    public:
    // A struct to describe blobs
    struct properties
    {
        cv::Point position;
        cv::Point speed;
        cv::Vec3b color;
        float orientation;
        float size;
    };

    public:
        Blob();
        ~Blob();

        int getId() {return mId;};

        void init(properties pNewBlob);
        properties predict();
        void setNewMeasures(properties pNewBlob);
        
        properties getBlob();
        bool isUpdated();

    private:
        bool updated;
        
        properties mProperties;
        cv::KalmanFilter mFilter;

        int mId;
};

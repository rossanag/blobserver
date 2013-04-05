/*
 * Copyright (C) 2012 Emmanuel Durand
 *
 * This file is part of blobserver.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * switcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with switcher.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @detector.h
 * The detector base class.
 */

#ifndef DETECTOR_H
#define DETECTOR_H

#include <algorithm>

#include "atom/message.h"
#include "opencv2/opencv.hpp"

#include "blob.h"
#include "source.h"

using namespace std;

/*************/
// Class for parallel masking
template <typename PixType>
class Parallel_Mask : public cv::ParallelLoopBody
{
    public:
        Parallel_Mask(cv::Mat* buffer, cv::Mat* mask):
            _buffer(buffer), _mask(mask) {}

        void operator()(const cv::Range& r) const
        {
            PixType* buffer = &(_buffer->at<PixType>(r.start, 0));
            PixType* mask = &(_mask->at<uchar>(r.start, 0));
            for (int y = r.start; y != r.end; ++y, buffer += _buffer->cols*sizeof(PixType), mask += _mask->cols*sizeof(uchar))
            {
                for (int x = 0; x < _buffer->cols; ++x)
                {
                    if (*(mask + x*sizeof(uchar)) == 0)
                        *(buffer + x*sizeof(PixType)) = PixType(0);
                }
            }
        }

    private:
        cv::Mat* _buffer;
        cv::Mat* _mask;
};

/*************/
//! Base Detector class, from which all detectors derive
class Detector
{
    public:
        /**
         * \brief Constructor
         */
        Detector();
        Detector(int pParam);

        /**
         * \brief Gets the class name of the detector
         */
        static string getClassName() {return mClassName;}
        /**
         * \brief Gets the class documentation of the detector
         */
        static string getDocumentation() {return mDocumentation;}
        /**
         * \brief Get the number of sources this detector needs
         */
        static unsigned int getSourceNbr() {return mSourceNbr;}

        /**
         * Detects objects in the capture given as a parameter, and returns a message with informations about each blob
         * The two first values in the message are the number of blob, and the size of each blob in the message
         * \param pCaptures A vector containing all captures. Their number should match mSourceNbr.
         */
        virtual atom::Message detect(vector<cv::Mat> pCaptures) {}
        
        /**
         * \brief Returns the message from the last call to detect()
         */
        atom::Message getLastMessage() {return mLastMessage;}

        /**
         * \brief Sets the mask to use on detection
         */
        void setMask(cv::Mat pMask);
        
        /**
         * \brief Sets a parameter
         * \param pParam A message containing the name of the parameter, and its desired value
         */
        virtual void setParameter(atom::Message pParam) {}

        /**
         * \brief Gets the current value for a given parameter
         * \param pParam A message containing the name of the parameter
         * \return Returns a message containing the name of the parameter and its current value
         */
        atom::Message getParameter(atom::Message pParam);
        
        /**
         * \brief Gives a ptr to the detector, for it to control the source (if needed)
         * \param source A shared_ptr to the source. A weak_ptr is created from it.
         */
        void addSource(shared_ptr<Source> source);
        
        /**
         * \brief Gets the name to use in the osc path when sending the message related to this detector
         */
        string getName() {return mName;}

        /**
         * \brief Gets the full OSC path to use for sending message from this detector
         */
        string getOscPath() {return mOscPath;}

        /**
         * \brief Gets the resulting image from the detector.
         */
        cv::Mat getOutput() {return mOutputBuffer.clone();}

    protected:
        cv::Mat mOutputBuffer; //!< The output buffer, resulting from the detection
        atom::Message mLastMessage; //!< Last message built by detect()
        bool mVerbose;

        string mOscPath; //!< OSC path for the detector, to be set in child class
        string mName; // !< Name of the detector, to be set in child class

        vector<weak_ptr<Source>> mSources;

        cv::Mat getMask(cv::Mat pCapture, int pInterpolation = CV_INTER_NN);
        void setBaseParameter(atom::Message pParam);

    private:
        static string mClassName; //!< Class name, to be set in child class
        static string mDocumentation; //!< Class documentation, to be set in child class
        static unsigned int mSourceNbr; //!< Number of sources needed for the detector, to be set in child class

        cv::Mat mSourceMask, mMask;

};

/*************/
// Useful functions
// trackBlobs is used to keep track of blobs through frames
/*************/
template<class T>
class BlobPair
{
    public:
        BlobPair(T* current, Blob::properties* measure):
            _current(current), _measure(measure)
        {
            _dist = _current->getDistanceFromPrediction(*_measure);
        }

        T* getCurrent() const {return _current;}
        Blob::properties* getMeasure() const {return _measure;}

        bool operator< (const BlobPair<T>& second) const
        {
            if (this->_dist > second.getDist())
                return true;
            else
                return false;
        }

        float getDist() const {return _dist;}

    private:
        T* _current;
        Blob::properties* _measure;
        float _dist;
};

/*************/
template<class T>
void trackBlobs(vector<Blob::properties> &pProperties, vector<T> &pBlobs, int pLifetime = 30)
{
    // First we update all the previous blobs we detected,
    // and keep their predicted new position
    for(int i = 0; i < pBlobs.size(); ++i)
        pBlobs[i].predict();
    
    // Then we compare all these prediction with real measures and
    // associate them together
    vector<BlobPair<T> > lPairs;
    if(pBlobs.size() != 0)
    {
        vector<BlobPair<T> > lSearchPairs;

        // Compute the squared distance between all new blobs, and all tracked ones
        for (int i = 0; i < pProperties.size(); ++i)
        {
            for (int j = 0; j < pBlobs.size(); ++j)
            {
                BlobPair<T> lPair(&pBlobs[j], &pProperties[i]);
                lSearchPairs.push_back(lPair);
            }
        }

        // We loop through the pairs to find the closest ones
        while (lSearchPairs.size())
        {
            make_heap(lSearchPairs.begin(), lSearchPairs.end());
            // Get the nearest new blob
            pop_heap(lSearchPairs.begin(), lSearchPairs.end());
            BlobPair<T> nearest = lSearchPairs.back();
            lSearchPairs.pop_back();
            lPairs.push_back(nearest);

            // Delete pairs with the same current blob
            // as well as pairs with the same new blob
            for (int j = 0; j < lSearchPairs.size();)
            {
                if (lSearchPairs[j].getCurrent() == nearest.getCurrent())
                    lSearchPairs.erase(lSearchPairs.begin() + j);
                else if (lSearchPairs[j].getMeasure() == nearest.getMeasure())
                    lSearchPairs.erase(lSearchPairs.begin() + j);
                else
                    j++;
            }
        }
    }

    // We update the blobs which we were able to track
    for (int i = 0; i < lPairs.size(); ++i)
    {
        lPairs[i].getCurrent()->setNewMeasures(*(lPairs[i].getMeasure()));
        lPairs[i].getCurrent()->renewLifetime();
    }
    // We delete the blobs we were not able to track
    for (int i = 0; i < pBlobs.size();)
    {
        bool isIn = false;
        for (int j = 0; j < lPairs.size(); ++j)
        {
            if (lPairs[j].getCurrent() == &pBlobs[i])
                isIn = true;
        }

        if (!isIn)
        {
            pBlobs[i].getOlder();
            if (pBlobs[i].getLifetime() < 0)
            {
                pBlobs.erase(pBlobs.begin() + i);
            }
            else
                i++;
        }
        else
            i++;
    }
    // And we create new blobs for the new objects detected
    for (int i = 0; i < pProperties.size(); ++i)
    {
        bool isIn = false;
        for (int j = 0; j < lPairs.size(); ++j)
        {
            if (lPairs[j].getMeasure() == &pProperties[i])
                isIn = true;
        }

        if (!isIn)
        {
            T newBlob;
            newBlob.init(pProperties[i]);
            newBlob.setLifetime(pLifetime);
            pBlobs.push_back(newBlob);
        }
    }
}

#endif // DETECTOR_H

/*******************************************************************************
 * Copyright (c) 2014, 2015  IBM Corporation and others
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/

#ifndef EigenSerializeUtils_h
#define EigenSerializeUtils_h

#ifdef ANDROID_STL_EXT
#include "string_ext.hpp"
#endif /* ANDROID_STL_EXT */
#include <Eigen/Core>

#include <cereal/archives/xml.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>

namespace cereal{
    /**
     Funtions to save/load Eigen::Matrix
     **/
    template<class Archive, class _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
    void save(Archive& archive, Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> const & X){
        int rows = static_cast<int>(X.rows());
        int cols = static_cast<int>(X.cols());
        archive(CEREAL_NVP(rows));
        archive(CEREAL_NVP(cols));
        std::vector<double> elements(rows*cols);
        Eigen::Map<Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>>(&elements[0], rows, cols ) = X;
        archive( CEREAL_NVP(elements));
    }
    
    template<class Archive, class _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
    void load(Archive & archive, Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> & X)
    {
        int rows;
        int cols;
        archive(CEREAL_NVP(rows));
        archive(CEREAL_NVP(cols));
        std::vector<double> elements(rows*cols);
        archive(CEREAL_NVP(elements));
        X = Eigen::Map<Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>>(&elements[0], rows, cols);
    }
}
#endif /* EigenSerializeUtils_h */

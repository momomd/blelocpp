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

#ifndef GaussianProcessLDPLMultiModel_hpp
#define GaussianProcessLDPLMultiModel_hpp

#include <stdio.h>
#include <cmath>

#include <boost/bimap/bimap.hpp>

#include "bleloc.h"
#include "KernelFunction.hpp"
#include "GaussianProcess.hpp"
#include "ObservationModel.hpp"
#include "ObservationModelTrainer.hpp"

namespace loc{
    
    /**
     ITUModelFunction
     **/
    
    enum GPType{
        GPNORMAL,
        GPLIGHT
    };
    
    class ITUModelFunction{
    
    private:
        double distanceOffset_=1.0;
    public:
        using Ptr = std::shared_ptr<ITUModelFunction>;
        
        static const int ndim_ = 4;
        int ndim(){return ndim_;}
        
        ITUModelFunction& distanceOffset(double distanceOffset);
        //void transformFeature(const Location& stateReceiver, const Location& stateTransmitter, double features[]) const;
        std::vector<double> transformFeature(const Location& stateReceiver, const Location& stateTransmitter) const;
        double predict(const double parameters[], const double features[]) const;
        double predict(const std::vector<double>& parameters, const std::vector<double>& features) const;
        
        template<class Archive>
        void serialize(Archive& ar);
    };
    
    /**
      GaussianProcess based model
     **/
    struct GaussianProcessLDPLMultiModelParameters{
        // Parameters for optimization
        int maxIteration_ = 10000;
        double tolranceOptimization_ = 0.00001;
        
        std::vector<double> lambdas{1000.0, 0.001, 1000, 1000};
        std::vector<double> rhos{0, 0, 100, 100};
        
    };
    
    template<class Tstate, class Tinput>
    class GaussianProcessLDPLMultiModel;
    
    template<class Tstate, class Tinput>
    class GaussianProcessLDPLMultiModelTrainer;
    
    template<class Tstate, class Tinput>
    class GaussianProcessLDPLMultiModel : public ObservationModel<Tstate, Tinput>{
    private:

        GaussianProcessLDPLMultiModelParameters trainParams;
        
        //std::shared_ptr<KernelFunction> mKernel;
        BLEBeacons mBLEBeacons;
        
        //ITUModelFunction mITUModel;
        std::map<int64_t, ITUModelFunction> mITUModelMap;
        
        std::vector<std::vector<double>> mITUParameters;
        
        //GaussianProcess mGP;
        std::shared_ptr<GaussianProcess> mGP;
        std::map<long, int> mBeaconIdIndexMap;
        //boost::bimaps::bimap<long, int> mBeaconIdIndexBimap;
        std::vector<double> mRssiStandardDeviations;
        bool mFillsUnknownBeaconRssi = false;
        double mStdevRssiForUnknownBeacon = 0.0;
        double computeNormalStandardDeviation(std::vector<double> standardDeviations);
        double mCoeffDiffFloorStdev = 5.0;
        
        // Private function to train the model
        //GaussianProcessLDPLMultiModel& kernelFunction(std::shared_ptr<KernelFunction> kernel);
        GaussianProcessLDPLMultiModel& bleBeacons(BLEBeacons bleBeacons);
        GaussianProcessLDPLMultiModel& train(Samples samples);
        std::vector<std::vector<double>> fitITUModel(Samples samples);
        std::vector<double> computeRssiStandardDeviations(Samples samples);
        std::vector<int> extractKnownBeaconIndices(const Tinput& beacons) const;
        
        friend class GaussianProcessLDPLMultiModelTrainer<Tstate, Tinput>;
        int version = 2;
        GPType gpType = GPNORMAL;
        
    public:
        GaussianProcessLDPLMultiModel() = default;
        ~GaussianProcessLDPLMultiModel() = default;
        
        std::function<double(double, double, double)> normFunc = MathUtils::logProbaNormal;
        
        std::vector<Tstate>* update(const std::vector<Tstate> & states, const Tinput & input) override {
            std::cout << "GaussianProcessLDPLMultiModel::update is not supported." << std::endl;
            std::vector<Tstate>* statesCopy = new std::vector<Tstate>(states);
            return statesCopy;
        }
        
        Tinput convertInput(const Tinput& input);
        // predict mean and stdev given state for input beacon id
        std::map<long, NormalParameter> predict(const Tstate& state, const Tinput& input) const;
        
        double computeLogLikelihood(const Tstate& state, const Tinput& input);
        std::vector<double> computeLogLikelihood(const std::vector<Tstate> & states, const Tinput& input) override;
        
        std::vector<double> computeLogLikelihoodRelatedValues(const Tstate& state, const Tinput& input);
        std::vector<std::vector<double>> computeLogLikelihoodRelatedValues(const std::vector<Tstate> & states, const Tinput& input) override;
        
        GaussianProcessLDPLMultiModel& fillsUnknownBeaconRssi(bool fills);
        bool fillsUnknownBeaconRssi() const;
        
        GaussianProcessLDPLMultiModel& rssiStandardDeviationForUnknownBeacons(double stdevRssi);
        double rssiStandardDeviationForUnknownBeacons() const;
        
        GaussianProcessLDPLMultiModel& coeffDiffFloorStdev(double);
        
        template<class Archive>
        void save(Archive& ar) const;
        template<class Archive>
        void load(Archive& ar);
        
        void save(std::ofstream& ofs) const;
        void save(std::ostringstream& oss) const;
        void load(std::ifstream& ifs);
        void load(std::istringstream& iss);

    };
    
    
    class MLAdapter{
    public:
        static std::vector<double> locationToVec(const Location& loc){
            std::vector<double> vec{loc.x(), loc.y(), loc.z(), loc.floor()};
            return vec;
        }
    };
    
    /**
     Definition of trainer
     **/
    template<class Tstate, class Tinput>
    class GaussianProcessLDPLMultiModelTrainer : public ObservationModelTrainer<Tstate, Tinput>{
        
    public:
        ~GaussianProcessLDPLMultiModelTrainer() override {}
        GaussianProcessLDPLMultiModel<Tstate, Tinput>* train() override;
        GaussianProcessLDPLMultiModelTrainer<Tstate, Tinput>& dataStore(std::shared_ptr<DataStore> dataStore) override{
            mDataStore = dataStore;
            return *this;
        }
        
        GPType gpType = GPNORMAL;
        
    private:
        std::shared_ptr<DataStore> mDataStore;
    };
    
}
#endif /* GaussianProcessLDPLMultiModel_hpp */

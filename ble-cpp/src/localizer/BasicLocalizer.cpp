/*******************************************************************************
 * Copyright (c) 2014, 2016  IBM Corporation and others
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

#include "BasicLocalizer.hpp"
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include <iostream>
#include <chrono>
#include "LogUtil.hpp"
#include "WeakPoseRandomWalker.hpp"

namespace loc{
    BasicLocalizer::BasicLocalizer(){
    }
    BasicLocalizer::~BasicLocalizer(){
    }
    
    
    StreamLocalizer& BasicLocalizer::updateHandler(void (*functionCalledAfterUpdate)(Status*)) {
        mFunctionCalledAfterUpdate = functionCalledAfterUpdate;
        if (mLocalizer) {
            mLocalizer->updateHandler(mFunctionCalledAfterUpdate);
        }
        return *this;
    }
    
    StreamLocalizer& BasicLocalizer::updateHandler(void (*functionCalledAfterUpdate)(void*, Status*), void* inUserData) {
        mFunctionCalledAfterUpdate2 = functionCalledAfterUpdate;
        mUserData = inUserData;
        if (mLocalizer) {
            mLocalizer->updateHandler(mFunctionCalledAfterUpdate2, mUserData);
        }
        return *this;
    }
    
    StreamLocalizer& BasicLocalizer::logHandler(void (*functionCalledToLog)(void*, std::string), void* inUserData) {
        mFunctionCalledToLog = functionCalledToLog;
        mUserDataToLog = inUserData;
        return *this;
    }
    
    StreamLocalizer& BasicLocalizer::putAttitude(const Attitude attitude) {
        if (!isReady) {
            return *this;
        }
        if (mFunctionCalledToLog) {
            mFunctionCalledToLog(mUserDataToLog, LogUtil::toString(attitude));
        }
        if (!isTrackingMode()) {
            return *this;
        }
        switch(mState) {
            case UNKNOWN: case LOCATING:
            case TRACKING:
                mLocalizer->putAttitude(attitude);
        }
        return *this;
    }
    StreamLocalizer& BasicLocalizer::putAcceleration(const Acceleration acceleration) {
        if (!isReady) {
            return *this;
        }
        if (mFunctionCalledToLog) {
            mFunctionCalledToLog(mUserDataToLog, LogUtil::toString(acceleration));
        }
        if (!isTrackingMode()) {
            return *this;
        }
        switch(mState) {
            case UNKNOWN: case LOCATING:
            case TRACKING:
                mLocalizer->putAcceleration(acceleration);
        }
        return *this;
    }
    
    StreamLocalizer& BasicLocalizer::putBeacons(const Beacons beacons) {
        if (!isReady) {
            return *this;
        }
        if (mFunctionCalledToLog) {
            mFunctionCalledToLog(mUserDataToLog, LogUtil::toString(beacons));
        }
        if (beaconFilter->filter(beacons).size() ==0){
            std::cout << "The number of strong beacon is zero." << std::endl;
            return *this;
        }
        if (smoothType == SMOOTH_RSSI) {
            beacons_list[(smooth_count++)%MIN(N_SMOOTH_MAX,nSmooth)] = beacons;
            
            std::map<long, loc::Beacons> allBeacons;
            
            for(int i = 0; i < N_SMOOTH_MAX && i < smooth_count && i < nSmooth; i++) {
                for(auto& b: beacons_list[i]) {
                    if (b.rssi() == 0) {
                        continue;
                    }
                    auto iter = allBeacons.find(b.id());
                    if (iter == allBeacons.end()) {
                        auto bs = loc::Beacons();
                        bs.insert(bs.end(), b);
                        allBeacons.insert(std::make_pair(b.id(), bs));
                    } else {
                        iter->second.insert(iter->second.end(), b);
                    }
                }
            }
            
            Beacons newBeacons;
            
            for(auto itr = allBeacons.begin(); itr != allBeacons.end(); ++itr) {
                auto& bs = itr->second;
                int c = 0;
                double rssi = 0;
                for(auto& b:bs) {
                    rssi += b.rssi();
                    c++;
                }
                newBeacons.insert(newBeacons.end(), loc::Beacon(bs[0].major(), bs[0].minor(), rssi/c));
            }
            
            switch(mState) {
                case UNKNOWN:
                    mLocalizer->resetStatus(newBeacons);
                    break;
                case LOCATING:
                    mLocalizer->resetStatus(newBeacons);
                    break;
                case TRACKING:
                    mLocalizer->putBeacons(newBeacons);
                    break;
            }
        } else {
            
            switch(mState) {
                case UNKNOWN:
                    mLocalizer->resetStatus(beacons);
                    break;
                case LOCATING:
                    mLocalizer->resetStatus(beacons);
                    break;
                case TRACKING:
                    mLocalizer->putBeacons(beacons);
                    return *this;
            }
        }
        
        loc::Status *status = new loc::Status();
        
        if (smoothType == SMOOTH_LOCATION) {
            if (isTrackingMode() && smooth_count >= nSmooth) {
                nSmooth = nSmoothTracking;
            }
            
            std::shared_ptr<loc::Location> loc = mLocalizer->getStatus()->meanLocation();
            
            status_list[(smooth_count++)%MIN(N_SMOOTH_MAX,nSmooth)] = *mLocalizer->getStatus()->states().get();
            
            std::vector<loc::State> *states = new std::vector<loc::State>();
            double meanBias = 0;
            for(int i = 0; i < N_SMOOTH_MAX && i < smooth_count && i < nSmooth; i++) {
                for(auto& s: status_list[i]) {
                    states->push_back(s);
                    meanBias += s.rssiBias();
                }
            }
            mEstimatedRssiBias = meanBias / states->size();
            status->states(states);
            mResult.reset(status);
        } else {
            mResult.reset(getStatus());
        }

        if (mFunctionCalledAfterUpdate) {
            //mFunctionCalledAfterUpdate(mLocalizer->getStatus());
            mFunctionCalledAfterUpdate(mResult.get());
        }
        
        if (mFunctionCalledAfterUpdate2 && mUserData) {
            //mFunctionCalledAfterUpdate2(mUserData, mLocalizer->getStatus());
            mFunctionCalledAfterUpdate2(mUserData, mResult.get());
        }
        
        if (smooth_count == nSmooth && isTrackingMode()) {
            //loc::State mean = mResult->states()->at(0);
            loc::State mean = loc::State::weightedMean(*mResult->states());
            auto std = loc::Location::standardDeviation(*mResult->states());
            mean.floor(roundf(mean.floor()));
            //double stdevX = std;
            //double stdevY = std;
            loc::Pose stdevPose;
            stdevPose.x(std.x()).y(std.y()).orientation(10*M_PI);
            mLocalizer->resetStatus(mean, stdevPose);
            
            std::cout << "Reset=" << mean << ", STD=" << std << std::endl;
            
            mState = TRACKING;
        }
        
        return *this;
    }
    
    StreamLocalizer& BasicLocalizer::putHeading(const Heading heading) {
        // Pass
        return *this;
    }
    
    Status* BasicLocalizer::getStatus() {
        return mLocalizer->getStatus();
    }
    
    bool BasicLocalizer::resetStatus() {
        return mLocalizer->resetStatus();
    }
    bool BasicLocalizer::resetStatus(Pose pose) {
        return mLocalizer->resetStatus(pose);
    }
    bool BasicLocalizer::resetStatus(Pose meanPose, Pose stdevPose) {
        return mLocalizer->resetStatus(meanPose, stdevPose);
    }
    bool BasicLocalizer::resetStatus(const Beacons& beacons) {
        return mLocalizer->resetStatus(beacons);
    }
    
    bool BasicLocalizer::resetStatus(const Location& location, const Beacons& beacons) {
        bool ret = mLocalizer->resetStatus(location, beacons);
        double meanBias = 0;
        for(loc::State s: *mLocalizer->getStatus()->states()) {
            meanBias += s.rssiBias();
        }
        mEstimatedRssiBias = meanBias / mLocalizer->getStatus()->states()->size();

        return ret;
    }
    
    const picojson::value &get(const picojson::value::object &obj, std::string key){
        auto itr = obj.find(key);
        if (itr != obj.end()) {
            return itr->second;
        }
        throw "not found";
    }
    
    const std::string &getString(const picojson::value::object &obj, std::string key) {
        auto& value = get(obj, key);
        if (value.is<std::string>()) {
            return value.get<std::string>();
        }
        throw "non string value";
    }
    double getDouble(const picojson::value::object &obj, std::string key) {
        auto& value = get(obj, key);
        if (value.is<double>()) {
            return value.get<double>();
        }
        throw "non double value";
    }
    const picojson::value::object &getObject(const picojson::value::object &obj, std::string key) {
        auto& value = get(obj, key);
        if (value.is<picojson::value::object>()) {
            return value.get<picojson::value::object>();
        }
        throw "non object value";
    }
    const picojson::value::array &getArray(const picojson::value::object &obj, std::string key) {
        auto& value = get(obj, key);
        if (value.is<picojson::value::array>()) {
            return value.get<picojson::value::array>();
        }
        throw "non array value";
    }

    BasicLocalizer& BasicLocalizer::setModel(std::string modelPath, std::string workingDir) {
        auto s = std::chrono::system_clock::now();
        std::cerr << "start setModel" << std::endl;
        if (isReady) { // TODO support multiple models
            std::cerr << "Already model was set" << std::endl;
            return *this;
        }
        
        std::ifstream file;
        file.open(modelPath, std::ios::in);
        std::istreambuf_iterator<char> input(file);
        
        picojson::value v;
        std::string err;
        picojson::parse(v, input, std::istreambuf_iterator<char>(), &err);
        if (!err.empty()) {
            throw err+" with reading "+modelPath;
        }
        if (!v.is<picojson::object>()) {
            throw "invalid JSON";
        }
        file.close();
        auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
        std::cerr << "parse JSON: " << msec << "ms" << std::endl;
        
        picojson::value::object& json = v.get<picojson::object>();
        
        auto& anchor = getObject(json, "anchor");
        this->anchor.latlng.lat = getDouble(anchor, "latitude");
        this->anchor.latlng.lng = getDouble(anchor, "longitude");
        this->anchor.rotate = getDouble(anchor, "rotate");
        latLngConverter()->anchor(this->anchor);
        
        deserializedModel = std::shared_ptr<GaussianProcessLDPLMultiModel<State, Beacons>> (new GaussianProcessLDPLMultiModel<State, Beacons>());
        
        bool doTraining = true;
        try {
            auto& str = getString(json, "ObservationModelParameters");
            if (false) {
                std::string omppath = DataUtils::stringToFile(str, workingDir, "ObservationModelParameters");
                
                msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
                std::cerr << "save deserialized model: " << msec << "ms" << std::endl;
                
                std::cerr << omppath << std::endl;
                //std::istringstream ompss(str);
                std::ifstream ompss(omppath);
                //if (ompss) {
                std::cout << "loading" << std::endl;
                deserializedModel->load(ompss);
                std::cout << "loaded" << std::endl;
                //}
            } else {
                std::istringstream ompss(str);
                if (ompss) {
                    std::cout << "loading" << std::endl;
                    deserializedModel->load(ompss);
                    std::cout << "loaded" << std::endl;
                }
            }
            msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
            std::cerr << "load deserialized model: " << msec << "ms" << std::endl;
            doTraining = false;
        } catch(...) {
        }
        
        
        mLocalizer = std::shared_ptr<StreamParticleFilter>(new StreamParticleFilter());
        if (mFunctionCalledAfterUpdate2 && mUserData) {
            mLocalizer->updateHandler(mFunctionCalledAfterUpdate2, mUserData);
        }
        if (mFunctionCalledAfterUpdate) {
            mLocalizer->updateHandler(mFunctionCalledAfterUpdate);
        }
        userData.localizer = this;
        
        mLocalizer->numStates(nStates);
        mLocalizer->alphaWeaken(alphaWeaken);
        mLocalizer->locationStandardDeviationLowerBound(locLB);
        mLocalizer->optVerbose(isVerboseLocalizer);
        
        
        std::cout << "Create data store" << std::endl << std::endl;
        // Create data store
        dataStore = std::shared_ptr<DataStoreImpl> (new DataStoreImpl());
        
        // Building - change read order to reduce memory usage peak
        //ImageHolder::setMode(ImageHolderMode(heavy));
        BuildingBuilder buildingBuilder;
        
        auto& buildings = getArray(json, "layers");
        
        for(int floor_num = 0; floor_num < buildings.size(); floor_num++) {
            auto& building = buildings.at(floor_num).get<picojson::value::object>();
            auto& param = getObject(building, "param");
            double ppmx = getDouble(param, "ppmx");
            double ppmy = getDouble(param, "ppmy");
            double ppmz = getDouble(param, "ppmz");
            double originx = getDouble(param, "originx");
            double originy = getDouble(param, "originy");
            double originz = getDouble(param, "originz");
            CoordinateSystemParameters coordSysParams(ppmx, ppmy, ppmz, originx, originy, originz);

            auto& data = getString(building, "data");
            std::ostringstream ostr;
            ostr << floor_num << "floor.png";
            
            std::string path = DataUtils::stringToFile(data, workingDir, ostr.str());
            
            int fn = floor_num;
            if (!get(param, "floor").is<picojson::null>()) {
                fn = (int)getDouble(param, "floor");
            }
            
            buildingBuilder.addFloorCoordinateSystemParametersAndImagePath(fn, coordSysParams, path);
            
            msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
            std::cerr << "prepare floor model[" << floor_num << "]: " << msec << "ms" << std::endl;
        }
        dataStore->building(buildingBuilder.build());
        
        msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
        std::cerr << "build floor model: " << msec << "ms" << std::endl;
        
        // Sampling data
        
        // Samples samples;
        auto& samples = getArray(json, "samples");
        for(int i = 0; i < samples.size(); i++) {
            auto& sample = samples.at(i).get<picojson::value::object>();
            auto& data = getString(sample, "data");
            
            //std::string samplepath = DataUtils::stringToFile(data, workingDir);
            //std::ifstream is(samplepath);
            std::istringstream is(data);
            dataStore->readSamples(is);
        }
        {
            std::cerr << dataStore->getSamples().size() << " samples have been loaded" << std::endl;
        }
        msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
        std::cerr << "load sample data: " << msec << "ms" << std::endl;
        //dataStore->samples(samples);
        
        // BLE beacon locations
        
        BLEBeacons bleBeacons;
        auto& beacons = getArray(json, "beacons");
        for(int i = 0; i < beacons.size(); i++) {
            auto& beacon = beacons.at(i).get<picojson::value::object>();
            auto& data = getString(beacon, "data");
            
            std::istringstream is(data);
            BLEBeacons bleBeaconsTmp = DataUtils::csvBLEBeaconsToBLEBeacons(is);
            bleBeacons.insert(bleBeacons.end(), bleBeaconsTmp.begin(), bleBeaconsTmp.end());
        }
        dataStore->bleBeacons(bleBeacons);
        msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
        std::cerr << "load beacon data: " << msec << "ms" << std::endl;
        
        if(doTraining){
            std::cerr << "Training will be processed" << std::endl;
            // Train observation model
            std::shared_ptr<GaussianProcessLDPLMultiModelTrainer<State, Beacons>>obsModelTrainer( new GaussianProcessLDPLMultiModelTrainer<State, Beacons>());
            obsModelTrainer->dataStore(dataStore);
            std::shared_ptr<GaussianProcessLDPLMultiModel<State, Beacons>> obsModel( obsModelTrainer->train());
            //localizer->observationModel(obsModel);
            
            std::ostringstream oss;
            obsModel->save(oss);
            
            json["ObservationModelParameters"] = (picojson::value)oss.str();
            
            std::ofstream of;
            of.open(modelPath);
            of << v.serialize();
            of.close();
        }
        
        // Instantiate sensor data processors
        // Orientation
        orientationMeterAverageParameters.interval(0.1);
        orientationMeterAverageParameters.windowAveraging(0.1);
        orientationMeter = std::shared_ptr<OrientationMeter>(new OrientationMeterAverage(orientationMeterAverageParameters));

        
        // Pedometer
        // TODO
        pedometerWSParams.updatePeriod(0.1);
        pedometerWSParams.walkDetectSigmaThreshold(walkDetectSigmaThreshold);
        // END TODO
        pedometer = std::shared_ptr<Pedometer>(new PedometerWalkingState(pedometerWSParams));
        
        // Set dependency
        mLocalizer->orientationMeter(orientationMeter);
        mLocalizer->pedometer(pedometer);
        
        // Build System Model
        // TODO (PoseProperty and StateProperty)
        poseProperty.meanVelocity(meanVelocity);
        poseProperty.stdVelocity(stdVelocity);
        poseProperty.diffusionVelocity(diffusionVelocity);
        poseProperty.minVelocity(minVelocity);
        poseProperty.maxVelocity(maxVelocity);
        poseProperty.stdOrientation(stdOrientation/180.0*M_PI);
        poseProperty.stdX(1.0);
        poseProperty.stdY(1.0);
        
        //    stateProperty.meanRssiBias(0.0);
        stateProperty.meanRssiBias(0.0);
        stateProperty.stdRssiBias(stdRssiBias);
        stateProperty.diffusionRssiBias(diffusionRssiBias);
        stateProperty.diffusionOrientationBias(diffusionOrientationBias/180.0*M_PI);
        
        // END TODO
        
        // Build poseRandomWalker
        poseRandomWalker = std::shared_ptr<PoseRandomWalker>(new PoseRandomWalker());
        poseRandomWalkerProperty.orientationMeter(orientationMeter.get());
        poseRandomWalkerProperty.pedometer(pedometer.get());
        poseRandomWalkerProperty.angularVelocityLimit(angularVelocityLimit/180.0*M_PI);
        poseRandomWalker->setProperty(poseRandomWalkerProperty);
        poseRandomWalker->setPoseProperty(poseProperty);
        poseRandomWalker->setStateProperty(stateProperty);
        
        // Combine poseRandomWalker and building
        prwBuildingProperty = SystemModelInBuildingProperty::Ptr(new SystemModelInBuildingProperty);
        prwBuildingProperty->maxIncidenceAngle(maxIncidenceAngle/180.0*M_PI);
        //prwBuildingProperty.weightDecayRate(0.9);
        //prwBuildingProperty.weightDecayRate(0.96593632892); // this^20 = 0.5
        //prwBuildingProperty.weightDecayRate(0.93303299153); // this^10 = 0.5
        prwBuildingProperty->weightDecayRate(pow(0.5, 1.0/weightDecayHalfLife)); // this^5 = 0.5
        
        prwBuildingProperty->velocityRateFloor(velocityRateFloor);
        prwBuildingProperty->velocityRateElevator(velocityRateElevator);
        prwBuildingProperty->velocityRateStair(velocityRateStair);
        
        
        Building building = dataStore->getBuilding();
        Building::Ptr buildingPtr(new Building(building));
        poseRandomWalkerInBuilding = std::shared_ptr<PoseRandomWalkerInBuilding>(new PoseRandomWalkerInBuilding());
        poseRandomWalkerInBuilding->poseRandomWalker(poseRandomWalker);
        poseRandomWalkerInBuilding->building(buildingPtr);
        poseRandomWalkerInBuilding->poseRandomWalkerInBuildingProperty(prwBuildingProperty);
        
        RandomWalkerProperty::Ptr randomWalkerProperty(new RandomWalkerProperty);
        randomWalkerProperty->sigma = 0.25;
        randomWalker.reset(new RandomWalker<State, SystemModelInput>());
        randomWalker->setProperty(randomWalkerProperty);

        
        // Setup RandomWalkerMotion
        RandomWalkerMotionProperty::Ptr randomWalkerMotionProperty(new RandomWalkerMotionProperty);
        randomWalkerMotionProperty->pedometer(pedometer);
        randomWalkerMotionProperty->orientationMeter(orientationMeter);
        randomWalkerMotionProperty->usesAngularVelocityLimit(true);
        randomWalkerMotionProperty->angularVelocityLimit(angularVelocityLimit/180.0*M_PI);
        randomWalkerMotionProperty->sigmaStop = sigmaStop;
        randomWalkerMotionProperty->sigmaMove = sigmaMove;
        
        if (localizeMode == RANDOM_WALK_ACC_ATT) {
            mLocalizer->systemModel(poseRandomWalkerInBuilding);
        }else if(localizeMode == RANDOM_WALK_ACC){
            RandomWalkerMotion<State, SystemModelInput>::Ptr randomWalkerMotion(new RandomWalkerMotion<State, SystemModelInput>);
            randomWalkerMotion->setProperty(randomWalkerMotionProperty);
            // Setup SystemModelInBuilding
            SystemModelInBuilding<State, SystemModelInput>::Ptr rwMotionBldg(new SystemModelInBuilding<State, SystemModelInput>(
                                                                            randomWalkerMotion, buildingPtr, prwBuildingProperty) );
            mLocalizer->systemModel(rwMotionBldg);
        }
        else if (localizeMode == RANDOM_WALK) {
            mLocalizer->systemModel(randomWalker);
        }
        else if (localizeMode == WEAK_POSE_RANDOM_WALKER){
            WeakPoseRandomWalker<State, SystemModelInput>::Ptr wPRW(new WeakPoseRandomWalker<State, SystemModelInput>);
            wPRW->setProperty(randomWalkerMotionProperty);
            wPRW->setWeakPoseRandomWalkerProperty(WeakPoseRandomWalkerProperty::Ptr(new WeakPoseRandomWalkerProperty));
            auto posePropertyTmp = PoseProperty::Ptr(new PoseProperty(poseProperty));
            auto statePropertyTmp = StateProperty::Ptr(new StateProperty(stateProperty));
            wPRW->setPoseProperty(posePropertyTmp);
            wPRW->setStateProperty(statePropertyTmp);
            SystemModelInBuilding<State, SystemModelInput>::Ptr wPRWBldg(new SystemModelInBuilding<State, SystemModelInput>(
                                                                            wPRW, buildingPtr, prwBuildingProperty) );
            mLocalizer->systemModel(wPRWBldg);
        }
        
        // set resampler
        resampler = std::shared_ptr<Resampler<State>>(new GridResampler<State>());
        mLocalizer->resampler(resampler);
        
        // Set status initializer
        ////PoseProperty poseProperty;
        ////StateProperty stateProperty;
        
        statusInitializer = std::shared_ptr<StatusInitializerImpl>(new StatusInitializerImpl());
        statusInitializer->dataStore(dataStore)
        .poseProperty(poseProperty).stateProperty(stateProperty);
        mLocalizer->statusInitializer(statusInitializer);
        
        // Set localizer
        mLocalizer->observationModel(deserializedModel);
        
        // Beacon filter
        beaconFilter = std::shared_ptr<StrongestBeaconFilter>(new StrongestBeaconFilter());
        beaconFilter->nStrongest(10);
        mLocalizer->beaconFilter(beaconFilter);
        
        // Set standard deviation of Pose
        double stdevX = 0.25;
        double stdevY = 0.25;
        double orientation = 10*M_PI;
        stdevPose.x(stdevX).y(stdevY).orientation(orientation);
        
        // ObservationDependentInitializer
        obsDepInitializer = std::shared_ptr<MetropolisSampler<State, Beacons>>(new MetropolisSampler<State, Beacons>());
        
        obsDepInitializer->observationModel(deserializedModel);
        obsDepInitializer->statusInitializer(statusInitializer);
        msParams.burnIn = nBurnIn;
        msParams.radius2D = burnInRadius2D; // 10[m]
        msParams.interval = burnInInterval;
        msParams.withOrdering = true;
        msParams.initType = burnInInitType;

        obsDepInitializer->parameters(msParams);
        mLocalizer->observationDependentInitializer(obsDepInitializer);
        
        // Mixture settings
        // double mixProba = 0.001;

        StreamParticleFilter::MixtureParameters mixParams;
        mixParams.mixtureProbability = mixProba;
        mixParams.rejectDistance = rejectDistance;
        mixParams.rejectFloorDifference(rejectFloorDifference);
        mLocalizer->mixtureParameters(mixParams);
        msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-s).count();
        std::cerr << "finish setModel: " << msec << "ms" << std::endl;
        isReady = true;
        return *this;
    }
    
    double BasicLocalizer::estimatedRssiBias() {
        return mEstimatedRssiBias;
    }
    
    void BasicLocalizer::updateStateProperty() {
        if (poseRandomWalker) {
            poseRandomWalker->setStateProperty(stateProperty);
            poseRandomWalker->setProperty(poseRandomWalkerProperty);
        }
        if (statusInitializer) {
            statusInitializer->stateProperty(stateProperty);
        }
        
    }
    
    void BasicLocalizer::meanRssiBias(double b) {
        stateProperty.meanRssiBias(b);
        updateStateProperty();
    }

    void BasicLocalizer::minRssiBias(double b) {
        stateProperty.minRssiBias(b);
        updateStateProperty();
    }
    void BasicLocalizer::maxRssiBias(double b) {
        stateProperty.maxRssiBias(b);
        updateStateProperty();
    }
    /*
    void BasicLocalizer::angularVelocityLimit(double a) {
        poseRandomWalkerProperty.angularVelocityLimit(a);
        updateStateProperty();
    }
     */

    void BasicLocalizer::normalFunction(NormalFunction type, double option) {
        if (type == NORMAL) {
            deserializedModel->normFunc = MathUtils::logProbaNormal;
        }
        else if (type == TDIST) {
            deserializedModel->normFunc = MathUtils::logProbatDistFunc(option);
        }
        
    }
    
    LatLngConverter::Ptr BasicLocalizer::latLngConverter(){
        return latLngConverter_;
    }
    
    
}
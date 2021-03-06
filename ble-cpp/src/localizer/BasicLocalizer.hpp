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

#ifndef BasicLocalizer_hpp
#define BasicLocalizer_hpp

#include <iostream>
#include <string>

#include <boost/circular_buffer.hpp>

#include "StreamLocalizer.hpp"
#include "StreamParticleFilter.hpp"

#include "PoseRandomWalker.hpp"
#include "WeakPoseRandomWalker.hpp"

#include "GridResampler.hpp"
#include "StatusInitializerStub.hpp"
#include "StatusInitializerImpl.hpp"

#include "DataStore.hpp"
#include "DataStoreImpl.hpp"

#include "OrientationMeterAverage.hpp"
#include "PedometerWalkingState.hpp"

#include "GaussianProcessLDPLMultiModel.hpp"

// for pose random walker in building
#include "Building.hpp"
#include "RandomWalker.hpp"
#include "RandomWalkerMotion.hpp"
#include "SystemModelInBuilding.hpp"

#include "BeaconFilterChain.hpp"
#include "CleansingBeaconFilter.hpp"
#include "StrongestBeaconFilter.hpp"

#include "ObservationDependentInitializer.hpp"
#include "MetropolisSampler.hpp"

#include "SerializeUtils.hpp"
#include "LatLngConverter.hpp"

#define N_SMOOTH_MAX 10

namespace loc {
    
    class BasicLocalizer;
    
    typedef struct {
        BasicLocalizer* localizer;
    } UserData;
    
    typedef enum {
        ONESHOT,
        RANDOM_WALK,
        RANDOM_WALK_ACC,
        RANDOM_WALK_ACC_ATT,
        WEAK_POSE_RANDOM_WALKER
    } LocalizeMode;
    
    /*
    typedef enum {
        UNKNOWN,
        LOCATING,
        TRACKING
    } LocalizeState;
    */
    
    typedef enum {
        NORMAL, TDIST
    } NormalFunction;
    
    typedef enum {
        SMOOTH_LOCATION,
        SMOOTH_RSSI
    } SmoothType;
    
    typedef enum{
        RAW_AVERAGE,
        TRANSFORMED_AVERAGE
    } OrientationMeterType;
    
    class LocalHeadingBuffer{
    protected:
        boost::circular_buffer<LocalHeading> buffer_;
        std::mutex mtx_;
    public:
        LocalHeadingBuffer(size_t);
        ~LocalHeadingBuffer() = default;
        LocalHeadingBuffer(const LocalHeadingBuffer&);
        LocalHeadingBuffer& operator=(const LocalHeadingBuffer&);
        void push_back(const LocalHeading&);
        LocalHeading& back();
        size_t size();
    };
    
    
    struct UserDataBridge{
        UserData* userData;
        void (*functionCalledAfterUpdateWithPtr)(void*, Status*) = NULL;
        BasicLocalizer* basicLocalizer;
    };
    
    class BasicLocalizer: public StreamLocalizer{
        
    private:
        std::shared_ptr<StreamParticleFilter> mLocalizer;
        std::shared_ptr<GaussianProcessLDPLMultiModel<State, Beacons>> deserializedModel;
        UserData userData;
        double isReady = false;
        
        void (*mFunctionCalledAfterUpdate)(Status*) = NULL;
        void (*mFunctionCalledAfterUpdate2)(void*, Status*) = NULL;
        void (*mFunctionCalledToLog)(void*, std::string) = NULL;
        void *mUserData = NULL;
        void *mUserDataToLog = NULL;
        
        
        UserDataBridge userDataBridge;
        void *mUserDataBridge = NULL;
        
        //std::shared_ptr<loc::Status> mResult;
        std::shared_ptr<Status> mTrackedStatus;
        
        std::vector<loc::State> status_list[N_SMOOTH_MAX];
        std::vector<loc::Beacon> beacons_list[N_SMOOTH_MAX];
        
        int smooth_count = 0;
        double mEstimatedRssiBias = 0;
        
        bool isTrackingLocalizer() {
            switch(localizeMode) {
                case ONESHOT:
                    return false;
                case RANDOM_WALK:
                case RANDOM_WALK_ACC:
                case RANDOM_WALK_ACC_ATT:
                case WEAK_POSE_RANDOM_WALKER:
                default:
                    return true;
            }
        };
        
        void updateStateProperty();
        
        Anchor anchor;
        LatLngConverter::Ptr latLngConverter_ = LatLngConverter::Ptr(new LatLngConverter());
        
        LocalHeadingBuffer mLocalHeadingBuffer = LocalHeadingBuffer(10); // buffer size 10 is not important.
        
        double headingConfidenceForOrientationInit_ = 0.0;
        
        Status::LocationStatus mLocationStatus = Status::UNKNOWN;
        
    public:
        BasicLocalizer();
        ~BasicLocalizer();
        
        int nStates = 1000;
        double alphaWeaken = 0.3;
        int nSmooth = 10;
        int nSmoothTracking = 1;
        SmoothType smoothType = SMOOTH_LOCATION;
        LocalizeMode localizeMode = ONESHOT;
        
        double effectiveSampleSizeThreshold = 1000;
        int nStrongest = 10;
        bool enablesFloorUpdate = true;
        
        double walkDetectSigmaThreshold = 0.6;
        double meanVelocity = 1.0;
        double stdVelocity = 0.3;
        double diffusionVelocity = 0.1;
        double minVelocity = 0.1;
        double maxVelocity = 1.5;
        
        //double meanRssiBias = 0.0;
        double stdRssiBias = 2.0;
        //double minRssiBias = -10;
        //double maxRssiBias = 10;
        double diffusionRssiBias = 0.2;
        double stdOrientation = 3.0;
        double diffusionOrientationBias = 10;
        double angularVelocityLimit = 30;
        // Parametes for PoseRandomWalker
        bool doesUpdateWhenStopping = false;
        
        double maxIncidenceAngle = 45;
        double weightDecayHalfLife = 5; // 0.87055056329; // this^5 = 0.5
        
        // Parameters for RandomWalkerMotion and WeakPoseRandomWalker
        double sigmaStop = 0.1;
        double sigmaMove = 1.0;
        
        // Parameters for SystemModelInBuilding
        double velocityRateFloor = 1.0;
        double velocityRateElevator = 0.5;
        double velocityRateStair = 0.5;
        double velocityRateEscalator = 0.5;
        double relativeVelocityEscalator = 0.4; // m/s
        
        // Parameters for WeakPoseRandomWalker
        double probabilityOrientationBiasJump = 0.1;
        double poseRandomWalkRate = 1.0;
        double randomWalkRate = 0.2;
        double probabilityBackwardMove = 0.0;
        
        int nBurnIn = 1000;
        int burnInRadius2D = 10;
        int burnInInterval = 1;
        InitType burnInInitType = INIT_WITH_SAMPLE_LOCATIONS;

        double mixProba = 0.000;
        double rejectDistance = 5;
        double rejectFloorDifference = 0.99;
        int nBeaconsMinimum = 3;
        
        Location locLB{0.5, 0.5, 1e-6, 1e-6};
        //Location locLB(0.5, 0.5, 0.0);
        bool isVerboseLocalizer = false;
        
        StreamParticleFilter::FloorTransitionParameters::Ptr pfFloorTransParams;
        LocationStatusMonitorParameters::Ptr locationStatusMonitorParameters = std::make_shared<LocationStatusMonitorParameters>();
        
        std::shared_ptr<DataStoreImpl> dataStore;
        
        void normalFunction(NormalFunction type, double option);
        void meanRssiBias(double b);
        void maxRssiBias(double b);
        void minRssiBias(double b);
        /*
         void angularVelocityLimit(double a);
         */
        double estimatedRssiBias();
        
        bool usesAltimeterForFloorTransCheck = false;
        double coeffDiffFloorStdev = 5.0;
        
        // parameters
        OrientationMeterAverageParameters orientationMeterAverageParameters;
        std::shared_ptr<OrientationMeter> orientationMeter;
        
        PedometerWalkingStateParameters pedometerWSParams;
        std::shared_ptr<Pedometer> pedometer;
        
        //PoseProperty poseProperty;
        //StateProperty stateProperty;
        PoseProperty::Ptr poseProperty = PoseProperty::Ptr(new PoseProperty);
        StateProperty::Ptr stateProperty = StateProperty::Ptr(new StateProperty);
        PoseRandomWalkerProperty::Ptr poseRandomWalkerProperty = PoseRandomWalkerProperty::Ptr(new PoseRandomWalkerProperty);
        WeakPoseRandomWalkerProperty::Ptr wPRWproperty = std::make_shared<WeakPoseRandomWalkerProperty>();
        std::shared_ptr<PoseRandomWalker>poseRandomWalker;
        std::shared_ptr<RandomWalker<State, SystemModelInput>>randomWalker;
        
        SystemModelInBuildingProperty::Ptr prwBuildingProperty = SystemModelInBuildingProperty::Ptr(new SystemModelInBuildingProperty);
        std::shared_ptr<PoseRandomWalkerInBuilding> poseRandomWalkerInBuilding;
        
        std::shared_ptr<Resampler<State>> resampler;
        
        std::shared_ptr<StatusInitializerImpl> statusInitializer;
        
        std::shared_ptr<StrongestBeaconFilter> beaconFilter;
        
        loc::Pose stdevPose;
        
        std::shared_ptr<MetropolisSampler<State, Beacons>> obsDepInitializer;
        
        MetropolisSampler<State, Beacons>::Parameters msParams;
        
        LatLngConverter::Ptr latLngConverter();

        StreamLocalizer& updateHandler(void (*functionCalledAfterUpdate)(Status*)) override;
        StreamLocalizer& updateHandler(void (*functionCalledAfterUpdate)(void*, Status*), void* inUserData) override;
        
        StreamLocalizer& logHandler(void (*functionCalledToLog)(void*, std::string), void* inUserData);

        StreamLocalizer& putAttitude(const Attitude attitude) override;
        StreamLocalizer& putAcceleration(const Acceleration acceleration) override;
        StreamLocalizer& putBeacons(const Beacons beacons) override;
        // StreamLocalizer& putWiFiAPs(const WiFiAPs wifiaps) override;
        StreamLocalizer& putLocalHeading(const LocalHeading heading) override;
        StreamLocalizer& putHeading(const Heading heading);
        StreamLocalizer& putAltimeter(const Altimeter altimeter) override;
        Status* getStatus() override;
                                 
        bool resetStatus() override;
        bool resetStatus(Pose pose) override;
        bool resetStatus(Pose meanPose, Pose stdevPose) override;
        bool resetStatus(Pose meanPose, Pose stdevPose, double rateContami) override;
        bool resetStatus(const Beacons& beacons) override;
        bool resetStatus(const Location& location, const Beacons& beacons) override;

        BasicLocalizer& setModel(std::string modelPath, std::string workingDir);
        
        bool tracksOrientation(){
            switch(localizeMode) {
                case ONESHOT:
                case RANDOM_WALK:
                case RANDOM_WALK_ACC:
                    return false;
                case RANDOM_WALK_ACC_ATT:
                case WEAK_POSE_RANDOM_WALKER:
                    return true;
                default:
                    return false;
            }
        };
        
        std::shared_ptr<GaussianProcessLDPLMultiModel<State, Beacons>> observationModel() const{
            return deserializedModel;
        };
        
        // for orientation initialization
        void headingConfidenceForOrientationInit(double confidence){
            if(!(0.0<=confidence && confidence<=1.0)){
                BOOST_THROW_EXCEPTION(LocException("range check error (0.0<=confidence && confidence<=1.0)"));
            }
            headingConfidenceForOrientationInit_ = confidence;
        }
        
        void updateLocationStatus(Status*);
        void overwriteLocationStatus(Status::LocationStatus);
        
        OrientationMeterType orientationMeterType = RAW_AVERAGE;
    };
}


#endif /* BasicLocalizerBuilder_hpp */

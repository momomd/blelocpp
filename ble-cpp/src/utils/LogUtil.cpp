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

#include "LogUtil.hpp"
#include "DataUtils.hpp"
#include <boost/format.hpp>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;
using namespace loc;

string LogUtil::toString(const Beacons& beacons) {
    std::stringbuf buffer;
    std::ostream os (&buffer);
    os << "Beacon," << beacons.size() << ",";
    for(const Beacon&b: beacons) {
        os << b.major() << "," << b.minor() << "," << b.rssi() << ",";
    }
    os << beacons.timestamp();
    return buffer.str();
}

string LogUtil::toString(const Acceleration& acc) {
    std::stringbuf buffer;
    std::ostream os (&buffer);
    os << "Acc," << acc.ax() << "," << acc.ay() << "," << acc.az() << "," << acc.timestamp();
    return buffer.str();
}

string LogUtil::toString(const Attitude& att) {
    std::stringbuf buffer;
    std::ostream os (&buffer);
    os << "Motion," << att.pitch() << "," << att.roll() << "," << att.yaw() << "," << att.timestamp();
    return buffer.str();
}

Beacons LogUtil::toBeacons(std::string str){
    return DataUtils::parseLogBeaconsCSV(str);
}

Acceleration LogUtil::toAcceleration(std::string str){
    std::vector<std::string> values;
    boost::split(values, str, boost::is_any_of(","));
    if(values.at(0).compare("Acc")!=0){
        BOOST_THROW_EXCEPTION(LocException("Log Acc is not correct. string="+str));
    }
    long timestamp = stol(values.at(4));
    Acceleration acc(timestamp,
                     stod(values.at(1)),
                     stod(values.at(2)),
                     stod(values.at(3))
                     );
    return acc;
}

Attitude LogUtil::toAttitude(std::string str){
    std::vector<std::string> values;
    boost::split(values, str, boost::is_any_of(","));
    if(values.at(0).compare("Motion")!=0){
        BOOST_THROW_EXCEPTION(LocException("Log Motion is not correct. string="+str));
    }
    // "Motion", pitch, roll, yaw
    double pitch = stod(values.at(1));
    double roll = stod(values.at(2));
    double yaw = stod(values.at(3));
    long timestamp = stol(values.at(4));
    Attitude att(timestamp, pitch, roll, yaw);// Attitude(timestamp, pitch, roll, yaw)
    return att;
}

#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <random>

#include "Street.h"
#include "Intersection.h"
#include "Vehicle.h"

std::mutex TrafficObject::_mtxCout;

// L3.1 : Safeguard all accesses to the private members _vehicles and _promises with an appropriate locking mechanism, 
// that will not cause a deadlock situation where access to the resources is accidentally blocked.

int WaitingVehicles::getSize()
{
    std::lock_guard<std::mutex> lock(_mutex);

    return _vehicles.size();
}

void WaitingVehicles::pushBack(std::shared_ptr<Vehicle> vehicle, std::promise<void> &&promise)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _vehicles.push_back(vehicle);
    _promises.push_back(std::move(promise));
}

void WaitingVehicles::permitEntryToFirstInQueue()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // L2.3 : First, get the entries from the front of _promises and _vehicles. 
    // Then, fulfill promise and send signal back that permission to enter has been granted.
    // Finally, remove the front elements from both queues. 

    auto firstVehiclePrms = _promises.begin();
    auto firstVehicle = _vehicles.begin();

    firstVehiclePrms->set_value(); //signal back

    _vehicles.erase(firstVehicle);
    _promises.erase(firstVehiclePrms);
}

Intersection::Intersection()
{
    _type = ObjectType::objectIntersection;
}

void Intersection::addStreet(std::shared_ptr<Street> street)
{
    _streets.push_back(street);
}

std::vector<std::shared_ptr<Street>> Intersection::queryStreets(std::shared_ptr<Street> incoming)
{
    // store all outgoing streets in a vector ...
    std::vector<std::shared_ptr<Street>> outgoings;
    for (auto it : _streets)
    {
        if (incoming->getID() != it->getID()) // ... except the street making the inquiry
        {
            outgoings.push_back(it);
        }
    }

    return outgoings;
}

bool Intersection::trafficLightIsGreen(){
    return _trafficLight.getCurrentPhase() == green;
}

void Intersection::addVehicleToQueue(std::shared_ptr<Vehicle> vehicle)
{
    // L3.3 : Ensure that the text output locks the console as a shared resource. Use the mutex _mtxCout you have added to the base class TrafficObject in the previous task. Make sure that in between the two calls to std-cout at the beginning and at the end of addVehicleToQueue the lock is not held. 

    std::unique_lock<std::mutex> lck(_mtxCout);
    std::cout << "Intersection #" << _id << "::addVehicleToQueue: thread id = " <<
        std::this_thread::get_id() << std::endl;
    lck.unlock();
    
    // L2.2 : First, add the new vehicle to the waiting line by creating a promise, a corresponding future and then adding both to _waitingVehicles. 
    // Then, wait until the vehicle has been granted entry. 

    std::promise<void> prmsVehicleAllowedToEnter;
    std::future<void> ftrVehicleAllowedToEnter = prmsVehicleAllowedToEnter.get_future();

    _waitingVehicles.pushBack(vehicle, std::move(prmsVehicleAllowedToEnter));
    ftrVehicleAllowedToEnter.wait();

    lck.lock();
        if(!trafficLightIsGreen()){
        _trafficLight.waitForGreen();
    }
    std::cout << "Intersection #" << _id << ": Vehicle #" << vehicle->getID() <<
        " is granted entry." << std::endl;
    lck.unlock();
}

void Intersection::vehicleHasLeft(std::shared_ptr<Vehicle> vehicle)
{
    this->setIsBlocked(false);
}

void Intersection::setIsBlocked(bool isBlocked)
{
    _isBlocked = isBlocked;
}

void Intersection::simulate()
{
    _trafficLight.simulate();
    threads.emplace_back(std::thread(&Intersection::processVehicleQueue, this));
}

void Intersection::processVehicleQueue()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (_waitingVehicles.getSize() > 0 && !_isBlocked)
        {
            // only enter one vehicle every time
            this->setIsBlocked(true);
            _waitingVehicles.permitEntryToFirstInQueue();
        }
    }
}
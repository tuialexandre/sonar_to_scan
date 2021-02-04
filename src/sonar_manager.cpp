#include <iostream>
#include <math.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include "sonar_manager.h"

SonarManager::SonarManager(ros::NodeHandle *n, std::string inputTopic, std::string outputTopic):
tfListener(tfBuffer){
	lscPub = n->advertise<sensor_msgs::LaserScan>(outputTopic, 1, this);
	lscSub = n->subscribe(inputTopic, 1, &SonarManager::lscCallback, this);
}

SonarManager::~SonarManager(){
	for(int i = 0; i < sensors.size(); i++)	{
		delete sensors[i];
	}
	sensors.clear();
}

void SonarManager::addSonar(ros::NodeHandle *n, std::string name, std::string frame){
	sonar* p = new sonar(n, name, frame);
	sensors.push_back(p);
}

void SonarManager::printSensors(){
	for(int i = 0; i < sensors.size(); i++){
		std::cout << sensors[i]->getTopic() << "  <-> " << sensors[i]->getFrame() << std::endl;
	}
}

void SonarManager::lscCallback(const sensor_msgs::LaserScanConstPtr& laser_input){
	sensor_msgs::LaserScan laser_output;

	laser_output.header = laser_input->header;
	laser_output.angle_min = -M_PI;
	laser_output.angle_max = M_PI;
	laser_output.angle_increment = laser_input->angle_increment;
	laser_output.time_increment = laser_input->time_increment;
	laser_output.scan_time = laser_input->scan_time;
	laser_output.range_min = laser_input->range_min;
	laser_output.range_max = laser_input->range_max;

	uint32_t ranges_size = std::ceil((laser_output.angle_max - laser_output.angle_min) / laser_output.angle_increment);
	laser_output.ranges.assign(ranges_size, std::numeric_limits<double>::infinity());

	//include the laser_input to laser_output
	for(int i = 0; i<laser_input->ranges.size(); i++)
	{
		int j = ((i * laser_input->angle_increment + laser_input->angle_min) - laser_output.angle_min)/laser_output.angle_increment;
		laser_output.ranges[j] = laser_input->ranges[i];
	}

	for(int i = 0; i < sensors.size(); i++){

		std::cout << "------------ "<< sensors[i]->getTopic() <<" ------------" << std:: endl;

		if(!sensors[i]->inLimits()){
	               	std::cout << "sensor out of sonar limits" << std::endl;
			continue;
		}

		//read the ultrasound position
		geometry_msgs::TransformStamped transform;
		try {
                transform = tfBuffer.lookupTransform(laser_input->header.frame_id,
                                                     sensors[i]->getFrame(),
                                                     ros::Time(0));
            	} catch (tf2::TransformException ex) {
                	ROS_WARN("tf error: %s", ex.what());
                	continue;
            	}

/*		include the ultrasounds to laser_output */
		geometry_msgs::PointStamped point_SensorFrame, point_LaserFrame;

		std::cout << "sonar range: " << sensors[i]->getRange() << std:: endl;

/*			include only the central point */
		point_SensorFrame.point.x = sensors[i]->getRange();
		tf2::doTransform(point_SensorFrame, point_LaserFrame, transform);	//tranform to laser frame
		includePointToLaser(point_LaserFrame, &laser_output);			//include the left point

/*			include all points of the cone
		for(float j=0; j < sensors[i]->getField()/4; j=j+0.01)
		{
			point_SensorFrame.point.x = sensors[i]->getRange();
			point_SensorFrame.point.y = tan(j)*sensors[i]->getRange();
			tf2::doTransform(point_SensorFrame, point_LaserFrame, transform);	//tranform to laser frame
			includePointToLaser(point_LaserFrame, &laser_output);			//include the left point

			point_SensorFrame.point.y *= -1;
			tf2::doTransform(point_SensorFrame, point_LaserFrame, transform);	//tranform to laser frame
			includePointToLaser(point_LaserFrame, &laser_output);			//include the right point
		}*/
	}

	lscPub.publish(laser_output);
}

void SonarManager::includePointToLaser(geometry_msgs::PointStamped point, sensor_msgs::LaserScan* laserScan){
	double range = hypot(point.point.y, point.point.x);
	double angle = atan2(point.point.y, point.point.x);

	if((angle >= laserScan->angle_min && angle <= laserScan->angle_max) && (range >= laserScan->range_min && range <= laserScan->range_max))
	{
		int index = (angle - laserScan->angle_min) / laserScan->angle_increment;
		if(laserScan->ranges[index] > range)
			laserScan->ranges[index] = range;
	}
	else
		std::cout << "fora dos limites do laser scan" << std:: endl
			  << "angle: " << angle << "range: " << range << std:: endl;

}
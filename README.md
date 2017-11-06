# Path planning project

## Objective
This is project 1 of term 3 in the Udacity Self-Driving-Car Nanodegree program. For this project, we were provided with a [simulator] (https://github.com/udacity/self-driving-car-sim/releases/tag/T3_v1.2) for highway driving and some starter code to *drive* our ego car in the simulator environment. Our objective was to write all the logic necessary to drive the ego car safely and efficiently. We had access to the map containing waypoints that represent the road (a three-lane highway). We also had access to sensor-fusion data that informed us of other cars (including velocity information) that were present in our vicinity. The car was initially in the middle lane and was stationary. The simulator corresponded through a websocket with our program every 0.2 seconds (simulation time) and the car is driven by providing a list of points to follow.

## Path Planning Overview
The basic framework detailed in the project-walkthrough video was followed. I used the spline library to describe a smooth path to follow, especially when changing lanes. I used frenet coordinates (s \& d) to represent the path and used the helper function *getXY* to convert to map coordinates. In this version being submitted, 100 waypoints are used to describe the future path.

Initially, I loop over all sensor_fusion cars in the vicinity and find out:
- If there is a blockage ahead (*blockage_ahead*) and the distance to blockage (*ds*)
- If left-lane is blocked (*left_blocked*) and distance to next car in left lane (*dblock_l*)
- If right-lane is blocked (*right_blocked*) and distance to next car in right lane (*dblock_r*)

```
	// ============================================================
	// check if car is in target lane
	if (icar_d >= (4*lane) && icar_d < (4*lane+4)) {

	  // check if time to collision is less than threshold
	  // (assuming the other car is stationary)
	  if (ds > 0 && ds < ds_t0) {
	    blockage_ahead = true;
	    if (ds < dblock) dblock = ds;
	  }
	}

	// ============================================================
	// check if car is in left lane
	if (icar_d >= (4*lane-4) && icar_d < (4*lane)) {
	  if (ds > -5) {
	    if (ds < ds_t1) left_blocked = true;
	    if (ds < dblock_l) dblock_l = ds;
	  }
	}

	// ============================================================
	// check if car is in right lane
	if (icar_d >= (4*lane+4) && icar_d < (4*lane+8)) {
	  if (ds > -5) {
	    if (ds < ds_t1) right_blocked = true;
	    if (ds < dblock_r) dblock_r = ds;
	  }
	}
```




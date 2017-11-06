# Path planning project

## Objective
This is project 1 of term 3 in the Udacity Self-Driving-Car Nanodegree program. For this project, we were provided with a [simulator](https://github.com/udacity/self-driving-car-sim/releases/tag/T3_v1.2) for highway driving and some starter code to *drive* our ego car in the simulator environment. Our objective was to write all the logic necessary to drive the ego car safely and efficiently. We had access to the map containing waypoints that represent the road (a three-lane highway). We also had access to sensor-fusion data that informed us of other cars (including velocity information) that were present in our vicinity. The car was initially in the middle lane and was stationary. The simulator corresponded through a websocket with our program every 0.2 seconds (simulation time) and the car is driven by providing a list of points to follow.

## Path Planning Overview
The basic framework detailed in the project-walkthrough video was followed. I used the spline library to describe a smooth path to follow, especially when changing lanes. I used frenet coordinates (s \& d) to represent the path and used the helper function *getXY* to convert to map coordinates. In this version being submitted, 100 waypoints are used to describe the future path.

Overall, I use a finite-state-machine to determine lane changes and target speed. Initially, I loop over all sensor_fusion cars in the vicinity and find out:
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

Based on this information, I decide whether to turn left, turn right or keep going straight and accordingly populate the booleans *turn_left* and *turn_right*. The logic to make this determination is  fairly straightforward as shown in the following piece of code.

```
      if (blockage_ahead) {

	bool turn_left = false;
	bool turn_right = false;

	// Try to change lane
	if (car_d>= 0 && car_d <= 4) {
	  // From left-most lane
	  if (!right_blocked) turn_right = true;

	} else if (car_d>= 4*(max_lanes-1) && car_d <= 4*max_lanes) {
	  // From right-most lane
	  if (!left_blocked) turn_left = true;

	} else {
	  // From middle lane
	  if (!left_blocked && !right_blocked) {

	    // Both are free. Check which one is more clear
	    if (dblock_l > dblock_r) turn_left = true;
	    else                     turn_right = true;
	  }
	  else if (!left_blocked)  turn_left = true;
	  else if (!right_blocked) turn_right = true;
	}
```

If a decision to turn was made, the variable *lane* is either decremented (left-turn) or incremented (right-turn) and the target speed is reduced. The car is also slowed down if there is blockage ahead and we are unable to turn lanes. If there is no blockage, the car is sped up until it reaches slightly below the speed limit.

Initially there were many successive lane changes and the car was snaking around too much. To mitigate this, I added a counter that started at zero and kept incrementing by one at the communication frequence and it was reset to zero whenever a lane-change occurred. After adding a constraint that this counter had to be at least 30 or so, before another lane-change can be initiated, the car was much more stable.

## Performance

The car has been driving reasonably well around the highway track, but it is not fool-proof. I have been able to complete the 4.35 mile loop multiple times, however sometimes incidents do occur especially when there is too much traffic and blockage ahead and no clear paths to change lanes exist.

Here is a [youtube video](https://youtu.be/-qm6gbvs-ZQ) showing successful driving performance:

<a href="http://www.youtube.com/watch?feature=player_embedded&v=-qm6gbvs-ZQ
" target="_blank"><img src="http://img.youtube.com/vi/-qm6gbvs-ZQ/0.jpg"
alt="T3P1" width="240" height="180" border="10" /></a>

Overall I believe the requirements specified in the rubric have been met. However the performance can certainly be improved. One approach could be to develop cost functions for every action that can be taken (with different weights for possible outcomes) and then choosing the optimal action that minimizes cost. 


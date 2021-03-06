#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

  double closestLen = 100000; //large number
  int closestWaypoint = 0;

  for(int i = 0; i < maps_x.size(); i++)
    {
      double map_x = maps_x[i];
      double map_y = maps_y[i];
      double dist = distance(x,y,map_x,map_y);
      if(dist < closestLen)
	{
	  closestLen = dist;
	  closestWaypoint = i;
	}

    }

  return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

  int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

  double map_x = maps_x[closestWaypoint];
  double map_y = maps_y[closestWaypoint];

  double heading = atan2( (map_y-y),(map_x-x) );

  double angle = abs(theta-heading);

  if(angle > pi()/4)
    {
      closestWaypoint++;
    }

  return closestWaypoint;

}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
  int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

  int prev_wp;
  prev_wp = next_wp-1;
  if(next_wp == 0)
    {
      prev_wp  = maps_x.size()-1;
    }

  double n_x = maps_x[next_wp]-maps_x[prev_wp];
  double n_y = maps_y[next_wp]-maps_y[prev_wp];
  double x_x = x - maps_x[prev_wp];
  double x_y = y - maps_y[prev_wp];

  // find the projection of x onto n
  double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
  double proj_x = proj_norm*n_x;
  double proj_y = proj_norm*n_y;

  double frenet_d = distance(x_x,x_y,proj_x,proj_y);

  //see if d value is positive or negative by comparing it to a center point

  double center_x = 1000-maps_x[prev_wp];
  double center_y = 2000-maps_y[prev_wp];
  double centerToPos = distance(center_x,center_y,x_x,x_y);
  double centerToRef = distance(center_x,center_y,proj_x,proj_y);

  if(centerToPos <= centerToRef)
    {
      frenet_d *= -1;
    }

  // calculate s value
  double frenet_s = 0;
  for(int i = 0; i < prev_wp; i++)
    {
      frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
    }

  frenet_s += distance(0,0,proj_x,proj_y);

  return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
  int prev_wp = -1;

  while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
    {
      prev_wp++;
    }

  int wp2 = (prev_wp+1)%maps_x.size();

  double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
  // the x,y,s along the segment
  double seg_s = (s-maps_s[prev_wp]);

  double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
  double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

  double perp_heading = heading-pi()/2;

  double x = seg_x + d*cos(perp_heading);
  double y = seg_y + d*sin(perp_heading);

  return {x,y};

}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";

  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

  // Start in lane 1
  int lane = 1;

  // Target velocity
  double ref_vel = 0.0;
  int nchange = 0;

  h.onMessage([&map_waypoints_x, &map_waypoints_y, &map_waypoints_s,
	       &map_waypoints_dx, &map_waypoints_dy, &lane, &ref_vel, &nchange]
	      (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length, uWS::OpCode opCode) {
		// "42" at the start of the message means there's a websocket message event.
		// The 4 signifies a websocket message
		// The 2 signifies a websocket event
		//auto sdata = string(data).substr(0, length);
		//cout << sdata << endl;
		if (length && length > 2 && data[0] == '4' && data[1] == '2') {

		  auto s = hasData(data);

		  if (s != "") {
		    auto j = json::parse(s);

		    string event = j[0].get<string>();

		    if (event == "telemetry") {

		      // Increment message number (will reset on lane change)
		      nchange += 1;

		      // Prevent changing lanes too frequently
		      bool ok_to_change = true;
		      if (nchange < 50) ok_to_change = false;

		      // Number of lanes available
		      const int max_lanes = 3;
		      const double max_speed = 49;

		      // j[1] is the data JSON object
		      // Main car's localization Data
		      double car_x = j[1]["x"];
		      double car_y = j[1]["y"];
		      double car_s = j[1]["s"];
		      double car_d = j[1]["d"];
		      double car_yaw = j[1]["yaw"];
		      double car_speed = j[1]["speed"];

		      // Previous path data given to the Planner
		      auto previous_path_x = j[1]["previous_path_x"];
		      auto previous_path_y = j[1]["previous_path_y"];

		      // Previous path's end s and d values
		      double end_path_s = j[1]["end_path_s"];
		      double end_path_d = j[1]["end_path_d"];

		      // Sensor Fusion Data, a list of all other cars on the same side of the road.
		      auto sensor_fusion = j[1]["sensor_fusion"];

		      json msgJson;

		      int prev_size = previous_path_x.size();

		      // ################################################################
		      // Check if we are getting too close
		      bool blockage_ahead = false;
		      int num_cars = sensor_fusion.size();

		      bool left_blocked = false;
		      bool right_blocked = false;
		      bool hard_brake = false;

		      double min_ds = 100;                   // Distance to nearest car (ahead)
		      int min_i = 0;                         // Index of nearest car

		      double dblock = 1000;                    // Distance to blockage ahead
		      double dblock_l = 1000;                  // Clear distance in left lane
		      double dblock_r = 1000;                  // Clear distance in right lane

		      for (int i=0; i<num_cars; i++) {
			// The other car's coordinates, velocity
			double icar_s = sensor_fusion[i][5];
			double icar_d = sensor_fusion[i][6];
			double icar_vx = sensor_fusion[i][3];
			double icar_vy = sensor_fusion[i][4];
			double icar_v = sqrt(icar_vx*icar_vx + icar_vy*icar_vy);


			double ds = icar_s - car_s;             // Relative distance
			double dv = car_speed*0.44704 - icar_v; // Relative velocity

			double ds_t0 = 30 + car_speed/50*20;       // Distance to keep (my lane)
			double ds_t1 = 0.02*75*car_speed*0.44704;  // Distance to keep (other lane)

			if (ds > 0 && ds < min_ds) {
			  min_ds = ds;
			  min_i = i;
			}

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
		      }

		      cout << "                                                   ";
		      cout << "\r BLOCKAGE: " << blockage_ahead ;
		      if (left_blocked ) {cout << " XXXX ";} else {cout << " ____ ";}
		      if (right_blocked) {cout << " XXXX ";} else {cout << " ____ ";}


		      bool slow_further = false;
		      bool turning = false;
		      // ============================================================
		      // TURNING
		      // ============================================================
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

			if (ok_to_change && turn_left) {
			  // Turning left
			  cout << " <<<< ";
			  ref_vel -= 0.5;
			  lane = max(0, lane-1);
			  nchange = 0;

			} else if (ok_to_change && turn_right) {
			  // turning right
			  cout << " >>>> ";
			  ref_vel -= 0.5;
			  lane = min(max_lanes-1, lane+1);
			  nchange = 0;

			} else {
			  // Not changing lanes. Slow down further
			  cout << " ____ ";
			  ref_vel -= 0.35;
			}

			if (hard_brake) {
			  cout << " !!!! ";
			  ref_vel = max(0.0, ref_vel-5);
			}
		      }
		      // ============================================================
		      // GOING STRAIGHT
		      // ============================================================
		      else if (ref_vel < max_speed) {
			cout << " ^^^^ ";
			ref_vel += 0.5;
		      }
		      else {
			cout << " .... ";
		      }

		      cout << " " << nchange;
		      cout << " " << dblock_l;
		      cout << " " << dblock_r;

		      fflush(stdout);

		      // Anchor points
		      vector<double> ptsx;
		      vector<double> ptsy;

		      double ref_x = car_x;
		      double ref_y = car_y;
		      double ref_yaw = deg2rad(car_yaw);

		      // ################################################################
		      // Setup anchor points [start with two previous]
		      if (prev_size >= 2) {

			double ref_x_prev = previous_path_x[prev_size-2];
			double ref_y_prev = previous_path_y[prev_size-2];

			// Update reference position to last point
			ref_x = previous_path_x[prev_size-1];
			ref_y = previous_path_y[prev_size-1];
			ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);

			ptsx.push_back(ref_x_prev);
			ptsx.push_back(ref_x);

			ptsy.push_back(ref_y_prev);
			ptsy.push_back(ref_y);
		      }

		      else {
			double prev_x = car_x - cos(ref_yaw);
			double prev_y = car_y - sin(ref_yaw);

			ptsx.push_back(prev_x);
			ptsx.push_back(car_x);

			ptsy.push_back(prev_y);
			ptsy.push_back(car_y);

		      }

		      // ################################################################
		      // Add a few forward anchor points
		      vector<double> next_pt;
		      double ds = ref_vel * 0.44704 * 3;
		      if (ds < 30) ds = 30;
		      if (ds > 70) ds = 70;
		      for (int i=1; i<=3; i++) {
			next_pt = getXY(car_s+i*ds, (2+4*lane),
					map_waypoints_s,
					map_waypoints_x,
					map_waypoints_y);
			ptsx.push_back(next_pt[0]);
			ptsy.push_back(next_pt[1]);
		      }


		      // ################################################################
		      // Convert to car-heading frame of reference
		      for (int i=0; i<ptsx.size(); i++) {
			double new_x = ptsx[i] - ref_x;
			double new_y = ptsy[i] - ref_y;

			ptsx[i] = new_x*cos(0-ref_yaw) - new_y*sin(0-ref_yaw);
			ptsy[i] = new_x*sin(0-ref_yaw) + new_y*cos(0-ref_yaw);

		      }

		      // ################################################################
		      // Spline fitting anchor points
		      tk::spline s;
		      s.set_points(ptsx, ptsy);

		      // ################################################################
		      vector<double> next_x_vals;
		      vector<double> next_y_vals;

		      // First let's fill previous points
		      for (int i=0; i<prev_size; i++) {
			next_x_vals.push_back(previous_path_x[i]);
			next_y_vals.push_back(previous_path_y[i]);
		      }

		      // ################################################################
		      // Fill up remaining with spline-fits

		      double target_x = 30.0;
		      double target_y = s(target_x);
		      double target_d = sqrt(target_x*target_x + target_y*target_y);

		      double N = target_d / (0.02*ref_vel*0.44707);
		      double x_addon = 0;

		      for (int i=0; i<100-prev_size; i++) {
			double x_point = x_addon + target_x/N;
			double y_point = s(x_point);

			x_addon = x_point;

			double new_x = ref_x + x_point*cos(ref_yaw) - y_point*sin(ref_yaw);
			double new_y = ref_y + x_point*sin(ref_yaw) + y_point*cos(ref_yaw);

			next_x_vals.push_back(new_x);
			next_y_vals.push_back(new_y);

		      }


		      msgJson["next_x"] = next_x_vals;
		      msgJson["next_y"] = next_y_vals;

		      auto msg = "42[\"control\","+ msgJson.dump()+"]";

		      //this_thread::sleep_for(chrono::milliseconds(1000));
		      ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);

		    }
		  } else {
		    // Manual driving
		    std::string msg = "42[\"manual\",{}]";
		    ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
		  }
		}
	      });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
		    const std::string s = "<h1>Hello world!</h1>";
		    if (req.getUrl().valueLength == 1) {
		      res->end(s.data(), s.length());
		    } else {
		      // i guess this should be done more gracefully?
		      res->end(nullptr, 0);
		    }
		  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
      std::cout << "Connected!!!" << std::endl;
    });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
		      ws.close();
		      std::cout << "Disconnected" << std::endl;
		    });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}

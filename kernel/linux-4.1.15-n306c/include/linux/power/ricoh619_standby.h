
int fl_level;		// If FL is on, value is 0-100. If FL is off, value is 0;
int fl_current;		// Unit is 1uA. If FL is off, value is 0;
int slp_state;		// 0:Suspend, 1:Hibernate
int idle_current;	// Unit is 1uA.
int sus_current;		// Unit is 1uA.
int hiber_current;	// Unit is 1uA.
bool bat_alert_req_flg;	// 0:Normal, 1:Re-synchronize request from system


# Radio Monitor

Radio Logger is a service that runs on raspberry pi (Ubuntu Server) that can connect and talk with remote equipment. The goal of the service is to provide useful insight in to the state of a remote RCO communications site, and provide that information back to the control AFSS through the RCE modem link. A summary of the goals are listed below:

- Provide serial numbers for installed radio equipment
- Provide information as to events that have occured
- Allow downloading of event data to the control site
- Show PTT status, RX squelch status, RF output power, RSSI levels, and other radio information both historically and real time
- Provide a point of power monitoring through GPIOs
- Show history of link status

A list of versions and changes can be found [here](Changelog.md).

A guide on the startup config file can be found [here](Config.md).
# Radio Monitor

Radio Monitor is a systemd service that runs on raspberry pi (Ubuntu Server) that can connect and talk with remote equipment. The goal of the service is to provide useful insight in to the state of a remote RCO communications site, and provide that information back to the control AFSS through the RCE modem link. A summary of the goals are listed below:

- Provide serial numbers for installed radio equipment
- Provide information as to events that have occured
- Allow downloading of event data to the control site
- Show PTT status, RX squelch status, RF output power, RSSI levels, and other radio information both historically and real time
- Provide a point of power monitoring through GPIOs
- Show history of link status

The service reads a config.json which determines how it operates. The config.json is checked for at startup. It is searched for in these locations, in this order:
1. On an attached usb drive
2. In the home directory (/home/ubuntu)
3. In the same directory as the executable

A daily status log is generated during execution in /home/ubuntu/status_logs.

Radio logs are generated in two possible locations:
- If usb drive is attached, csv radio logs are generated in csvlogs folder on usb drive
- Otherwise, generated in /ubuntu/home/csvlogs

Radio log format and generation is determined by the loaded config.json file. The filename of the generated log file is "Name_Of_Logger (Date).csv". This means, a new file is generated for every day of logging. If there are a lot of radios, and the logging period is short, there can be a huge amount of data per log.

When logging to usb drive, make sure to **stop the Radio Monitor service before removing the usb drive or turning off the PI**. This is because OS doesn't necessary write data to the usb drive the second its written to file: It buffers it and writes a bunch at once to avoid degrading the drive. Stopping Radio Monitor will unmount the usb drive, which will cause the OS to flush all data writes to the drive, and you won't loose any data.

The deployment package can be found [here](https://docs.google.com/document/d/1WjWND-U78ZBYJoIlb42E0mNrbUn4aCda/edit?usp=sharing&ouid=101072115658933791924&rtpof=true&sd=true).

A guide on the startup config file can be found [here](Config.md).

A list of versions and changes can be found [here](Changelog.md).
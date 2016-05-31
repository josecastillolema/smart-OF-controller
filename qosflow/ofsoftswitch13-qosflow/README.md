# QoSFlow Project

This software switch is based on OpenFlow 1.3 software switch developed by CPqD. QoSFlow is a proposal
that enables OpenFlow to control multiple packet-scheduler of Linux operational system.

# OpenFlow 1.3 Software Switch

This is an OpenFlow 1.3 compatible user-space software switch implementation. The code is based on the Ericsson TrafficLab 1.1 softswitch
implementation, with changes in the forwarding plane to support OpenFlow 1.3.

The following components are available in this package:

* ofdatapath: the switch implementation

* ofprotocol: secure channel for connecting the switch to the controller

* oflib: a library for converting to/from 1.3 wire format

* dpctl: a tool for configuring the switch from the console

## Before building
The switch makes use of the NetBee library to parse packets, so we need to install it first.

1. Install the following packages:

    ```
    $ sudo apt-get install cmake libpcap-dev libxerces-c2-dev libpcre3-dev flex bison
    ```

2. Download and unpack the source code from: http://www.nbee.org/download/nbeesrc-12-05-16.php

3. Create the build system

    ```
    $ cd nbeesrc/src
    $ cmake .
    ```

4. Compile

    ```
    $ make
    ```

5. Add the shared libraries built in /nbeesrc/bin/ to your /usr/local/lib directory

    ```
    $ sudo cp ../bin/libn*.so /usr/local/lib
    ```

6. Run ldconfig

    ```
    $ sudo ldconfig
    ```

7. Put the folder nbeesrc/include in the /usr/include

    ```
    $ sudo cp -R ../include /usr/
    ```

## Building
Run the following commands in the `of13softswitch` directory to build and install everything:

    $ ./boot.sh
    $ ./configure
    $ make
    $ sudo make install

## Running
1. Start the datapath:

    ```
    $ sudo udatapath/ofdatapath --datapath-id=<dpid> --interfaces=<if-list> ptcp:<port>
    ```

    This will start the datapath, with the given datapath ID, using the interaces listed. It will open a passive TCP connection on the given port. For a complete list of options, use the `--help` argument.

2. Start the secure channel, which will connect the datapath to the controller:

    ```
    $ secchan/ofprotocol tcp:<switch-host>:<switch-port> tcp:<ctrl-host>:<ctrl-port>
    ```

    This will open TCP connections to both the switch and the controller, relaying OpenFlow protocol messages between them. For a complete list of options, use the --help argument.

## Configuring
By using the dpctl utility, you can send messages to the switch for addind, deleting or modifying packet scheduler configuration.


* For HTB packet scheduler:

    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=add,port=1,queue=1 type=htb min=10000,max=12000
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=mod,port=1,queue=1 type=htb min=5000,max=5000
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port psched cmd=del,port=1,queue=1 type=htb 
    ```

* For SFQ packet scheduler:

    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=add,port=1,queue=1 type=sfq quantum=2,perturb=10
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=mod,port=1,queue=1 type=sfq quantum=2,perturb=15
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=del,port=1,queue=1
    ```

* For RED packet scheduler:

    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=add,port=1,queue=1 type=red min_len=500,max_len=1200,avpkt=200,limit=1500,rate=64
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=mod,port=1,queue=1 type=red min_len=500,max_len=1200,avpkt=200,limit=1500,rate=120
    ```


    ```
    $ utilities/dpctl tcp:<switch-host>:<switch-port> psched cmd=del,port=1,queue=1 type=red
    ```

For a complete list of commands and arguments, use the `--help` argument.


# Contribute
Please submit your bug reports, fixes and suggestions as pull requests on
GitHub, or by contacting us directly.

# License
OpenFlow 1.3 Software Switch is released under the BSD license (BSD-like for
code from the original Stanford switch).

# Contact

Airton Ishimori (airton@ufpa.br)

# More Information

For more information about OpenFlow 1.3, you can contact:

Eder Leao Fernandes (ederlf@cpqd.com.br)

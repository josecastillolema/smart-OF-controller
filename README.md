![GitHub](https://img.shields.io/github/license/josecastillolema/smart-OF-controller)
![GitHub language count](https://img.shields.io/github/languages/count/josecastillolema/smart-OF-controller)
![GitHub top language](https://img.shields.io/github/languages/top/josecastillolema/smart-OF-controller)
[![Requirements Status](https://requires.io/github/josecastillolema/smart-OF-controller/requirements.svg?branch=master)](https://requires.io/github/josecastillolema/smart-OF-controller/requirements/?branch=master)



# smart-OF-controller

This project aims to design and develop a clean-slate Future Internet framework called SMART (Support of Mobile Sessions with High Transport Network Resource Demand). In this framework, Software-Defined Networking mechanisms will be applied and fitted in vital aspects of the RNP (Brazilian National Research and Education Network) infrastructure, to provide a truly reliable and robust innovative provisioning system, by seeking to allow optimizing its operation and control to support mobile multimedia applications with guaranteed quality of service over the time. The SMART framework will act as a complementary communication service provider for the RNP with the following main innovations:
    (i) clean-slate Future Internet network architecture with new addressing methods, group-based connectivity, QoS-oriented mobility and resilience controls;
    (ii) IEEE 802.21 compliant signaling approach to control device handover;
    (iii) over-provisioning paradigm based automated, systematic and dynamic network resource allocation integrated with [OpenFlow](http://archive.openflow.org/wp/learnmore/);
    (iv) OpenFlow extensions to provide QoS support.

The details of the proposal regarding (i), (iii) and (iv) can be found in [this MSc Thesis](http://bdtd.ibict.br/vufind/Record/UFRN_7ccf2b703d54b0fd8cc548ccd747339a) and in [this paper](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=7063426) [![DOI:10.1109/GLOCOMW.2014.7063426](https://zenodo.org/badge/DOI/10.1109/GLOCOMW.2014.7063426.svg)](https://doi.org/10.1109/GLOCOMW.2014.7063426)
.

The details of the proposal regarding (ii) can be found in [this paper](http://www.sciencedirect.com/science/article/pii/S1389128616301177) [![DOI:10.1016/j.comnet.2016.04.019](https://zenodo.org/badge/DOI/10.1016/j.comnet.2016.04.019.svg)](https://doi.org/10.1016/j.comnet.2016.04.019).

In order to implement (iv) [QoSFlow](https://groups.google.com/a/openflowhub.org/forum/#!topic/floodlight-dev/C5Z_At7deRA) was choosen. The QoSFlow project is an open-source proposal for enabling control of multiple packet schedulers in OpenFlow network design. For that, the project uses Netlink library to open a communication channel with Traffic Control subsystem of Linux kernel where packet schedulers are located. Such schedulers, or usually called queueing disciplines, are responsible to handle packets that cross the network and based on each packet scheduler particularity, different treatment to the packets can be achieved. More information can be found in [this paper](http://dl.acm.org/citation.cfm?id=2570478).

QoSFlow for OpenFlow v1.0 can be found [here](https://bitbucket.org/airtoncomp/ofsoftswitch10-qosflow).
QoSFlow for OpenFlow v1.3 can be found [here](https://bitbucket.org/airtoncomp/ofsoftswitch13-qosflow).

Funding Institution: Conselho Nacional de Desenvolvimento Científico e Tecnológico (CNPq) project code 45705120140.

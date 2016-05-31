#include <jni.h>  
#include "br_ufu_facom_network_dlontology_FinSocket.h"  

#include <net/if.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <math.h>

#include <errno.h>

/*
 * Class:     FinSocket
 * Method:    finOpen
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_finOpen
  (JNIEnv * env , jobject obj){
  int s;

  s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (s == -1)
  	printf("socket error\n");

  return s;
}

/*
 * Class:     FinSocket
 * Method:    finClose
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_finClose
  (JNIEnv *env, jobject obj, jint sock){

  return close(sock);
}

/*
 * Class:     FinSocket
 * Method:    finWrite
 * Signature: (Ljava/lang/String;Ljava/lang/String;I)Z
 */
JNIEXPORT jboolean JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_finWrite
  (JNIEnv * env, jobject obj, jint ifIndex, jint soc, jbyteArray data, jint offset, jint len){
	int result;
	jbyte *buf;

        /*target address*/
        struct sockaddr_ll socket_address;

        /*we don't use a protocoll above ethernet layer
          ->just use anything here*/
        socket_address.sll_protocol = htons(ETH_P_ALL);

        socket_address.sll_ifindex  = ifIndex;

	//Tamanho do endereço
	socket_address.sll_halen = ETH_ALEN;

        buf = (*env)->GetByteArrayElements(env, data, NULL);

	if(finSelect(soc,0,5,0)<0)
		return 1;

  	result = sendto(soc, &buf[offset], len, 0,(struct sockaddr*)&socket_address, sizeof(socket_address));

	if(result < 0){
            printf("Error sending the packet: %d - Size %d - %s\n", errno, len,strerror( errno ) );	
	   // printf("Offset:%d\nLen:%d\nSTR:%s",offset,len,&buf[offset]);
	}

  	(*env)->ReleaseByteArrayElements(env, data, buf, JNI_ABORT);


	return result > 0;
	
}

JNIEXPORT jint JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_finRead
  (JNIEnv * env, jobject obj, jint soc, jbyteArray data, jint offset, jint len){
	
	jbyte *buf;
	int result;

	buf = (*env)->GetByteArrayElements(env, data, NULL);

	//result = recvfrom(soc, buf+offset, len, 0, NULL, NULL);
	result = recv(soc, buf+offset, len, 0);

  	(*env)->ReleaseByteArrayElements(env, data, buf, 0);

        return result;
}

JNIEXPORT jboolean JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_setPromiscousMode
  (JNIEnv * env, jobject obj, jstring ifName,  jint soc){
        struct ifreq ifr;


        // O procedimento abaixo é utilizado para "setar" a 
        // interface em modo promíscuo
	const char *ifNameChars = (*env)->GetStringUTFChars(env, ifName, 0);
        strcpy(ifr.ifr_name, ifNameChars);
	(*env)->ReleaseStringUTFChars(env, ifName, ifNameChars);

        if(ioctl(soc, SIOCGIFINDEX, &ifr) < 0) return 0;
	if(ioctl(soc, SIOCGIFFLAGS, &ifr) < 0) return 0;
        ifr.ifr_flags |= IFF_PROMISC;
        if(ioctl(soc, SIOCSIFFLAGS, &ifr) < 0) return 0;

	return 1;
}

int finSelect(int socket, int read, int seconds, int microseconds){
  int result;
  struct timeval timeout;
  fd_set *rset = NULL, *wset = NULL, errset, fdset;

  FD_ZERO(&fdset);
  FD_ZERO(&errset);
  FD_SET(socket, &fdset);
  FD_SET(socket, &errset);

  timeout.tv_sec  = seconds;
  timeout.tv_usec = microseconds;

  if(read){
	printf("Read! =O\n");
    rset = &fdset;
  }else
    wset = &fdset;

  result = select(socket + 1, rset, wset, &errset, &timeout);

  if(result >= 0) {
    if(FD_ISSET(socket, &errset)){
      result = -1;
    }else if(FD_ISSET(socket, &fdset))
      result = 0;
    else {
      result = -1;
    }
  }
  return result;
}

JNIEXPORT jobject JNICALL Java_br_ufu_facom_network_dlontology_FinSocket_getNetIfs
(JNIEnv * env, jobject obj){
	//Criando o MAP
	jclass mapClass = (*env)->FindClass(env, "java/util/HashMap");
	jclass intClass = (*env)->FindClass(env, "java/lang/Integer");
	jclass stringClass = (*env)->FindClass(env, "java/lang/String");

	if(mapClass == NULL || intClass == NULL || stringClass == NULL){
		return NULL;
	}
	
	jmethodID initMap = (*env)->GetMethodID(env, mapClass, "<init>", "()V");
	jmethodID initInt = (*env)->GetMethodID(env, intClass, "<init>", "(I)V");

	jobject hashMap = (*env)->NewObject(env, mapClass, initMap);

	jmethodID put = (*env)->GetMethodID(env, mapClass, "put","(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

	//Buscando as interfaces no sistema
	struct if_nameindex *pif;
	struct if_nameindex *head;
	head = pif = if_nameindex(); 
	while (pif->if_index) { 
	
		jobject index = (*env)->NewObject(env, intClass, initInt, pif->if_index);
		jstring name = (*env)->NewStringUTF(env, pif->if_name);

		(*env)->CallObjectMethod(env, hashMap, put, index, name);

		pif++; 
	} 

	//Limpando o ambiente
	if_freenameindex(head); 
	(*env)->DeleteLocalRef(env, mapClass);
	(*env)->DeleteLocalRef(env, intClass);

	return hashMap;
}




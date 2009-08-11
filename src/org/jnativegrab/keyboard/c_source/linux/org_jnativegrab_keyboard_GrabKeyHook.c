/*
 *	JNativeGrab - GrabKeyHook - 09/08/06
 *  Alexander Barker
 *
 *	JNI Interface for setting a Keyboard Hook and monitoring
 *	it with java.
 *
 *  TODO Add GPL License
 */

/*
Compiling Options:
	gcc -m32 -march=i586 -shared -fPIC -lX11 -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux ./org_jnativegrab_keyboard_GrabKeyHook.c -o libJNativeGrab_Keyboard.so
	gcc -m64 -march=i586 -shared -fPIC -lX11 -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux ./org_jnativegrab_keyboard_GrabKeyHook.c -o libJNativeGrab_Keyboard.so
*/

typedef enum { FALSE, TRUE } bool;

#ifdef DEBUG
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <jni.h>
#include "org_jnativegrab_keyboard_GrabKeyHook.h"



//Instance Variables
bool bRunning = TRUE;

Display * disp;
Window default_win;

XEvent xev;
XEvent xev_next;
JavaVM * jvm = NULL;
jobject hookObj = NULL;
jmethodID fireKeyPressed_ID = NULL;
jmethodID fireKeyReleased_ID = NULL;
pthread_t hookThreadId = 0;

//Shared Object Constructor and Deconstructor
void __attribute__ ((constructor)) Init(void);
void __attribute__ ((destructor)) Cleanup(void);

void throwException(JNIEnv * env, char * sMessage) {
	//Locate our exception class
	jclass objExceptionClass = (*env)->FindClass(env, "org/jnativegrab/keyboard/GrabKeyException");

	if (objExceptionClass != NULL) {
		#ifdef DEBUG
		printf("C++ Exception: %s\n", sMessage);
		#endif

		(*env)->ThrowNew(env, objExceptionClass, sMessage);
		//(*env)->ExceptionDescribe();
		//(*env)->DeleteLocalRef(objExceptionClass);
	}
	else {
		//Unable to find exception class
		#ifdef DEBUG
		printf("Native: Unable to locate exception class.\n");
		#endif

		//FIXME Terminate with error.  Maybe throw xerror
	}
}

void MsgLoop() {
	JNIEnv * env = NULL;
	(*jvm)->AttachCurrentThread(jvm, (void **)(&env), NULL);

	while (bRunning) {
		XNextEvent(disp, &xev);

		switch (xev.type) {
			case KeyPress:
				#ifdef DEBUG
				printf("Native: MsgLoop - Key pressed (%i)\n", iKeyCode);
				#endif

				(*env)->CallVoidMethod(env, hookObj, fireKeyPressed_ID, (jlong) xev.xkey.time, (jint) xev.xkey.state, (jint) xev.xkey.keycode, (jchar) xev.xkey.keycode);
			break;

			case KeyRelease:
				#ifdef DEBUG
				printf("Native: MsgLoop - Key released(%i)\n", iKeyCode);
				#endif

				(*env)->CallVoidMethod(env, hookObj, fireKeyReleased_ID, (jlong) xev.xkey.time, (jint) xev.xkey.state, (jint) xev.xkey.keycode, (jchar) xev.xkey.keycode);
			break;
		}
	}
}

JNIEXPORT void JNICALL Java_org_jnativegrab_keyboard_GrabKeyHook_grabKey(JNIEnv * env, jobject obj, jint modifiers, jint keycode) {
	//Attempt the grab the key.
	XGrabKey(disp, keycode, modifiers, default_win, True, GrabModeAsync, GrabModeAsync);


}


JNIEXPORT void JNICALL Java_org_jnativegrab_keyboard_GrabKeyHook_ungrabKey(JNIEnv *env, jobject object, jint modifiers, jint keycode) {
	XUngrabKey(disp, keycode, modifiers, default_win);

	#ifdef DEBUG
	printf("Native: XUngrabKey successful.\n");
	#endif
}

//This is where java attaches to the native machine.  Its kind of like the java + native constructor.
JNIEXPORT void JNICALL Java_org_jnativegrab_keyboard_GrabKeyHook_startHook(JNIEnv * env, jobject obj) {
	//Grab the currently running virtual machine so we can attach to it in
	//functions that are not called from java. ( I.E. MsgLoop )
	(*env)->GetJavaVM(env, &jvm);

	//FIXME This needs to be added.
	//Set the native error handler.
	//XSetErrorHandler((XErrorHandler) xErrorHandler);

	//Grab the default display
	char * disp_name = XDisplayName(NULL);
	disp = XOpenDisplay(disp_name);
	if (disp == NULL) {
		//We couldnt hook a display so we need to die.
		char * error_msg = "Could not open display: ";
		char * exceptoin_msg = (char *) malloc( (strlen(error_msg) + strlen(disp_name)) + 1 * sizeof(char));

		strcat(exceptoin_msg, error_msg);
		strcat(exceptoin_msg, disp_name);

		throwException(env, exceptoin_msg);
		free(exceptoin_msg);

		//Naturaly exit so jni exception is thrown.
		return;
	}

	#ifdef DEBUG
	printf("Native: XOpenDisplay(XDisplayName(NULL)) successful\n");
	#endif

	//Set allowed events and the default root window.
	XAllowEvents(disp, AsyncKeyboard, CurrentTime);
	default_win = DefaultRootWindow(disp);

	//Iterate over screens
	int screen;
	for (screen = 0; screen < ScreenCount (disp); screen++) {
		printf ("Init Screen %i\n", screen);
		XSelectInput(disp, RootWindow(disp, screen), KeyPressMask | KeyReleaseMask);
	}

	//Setup all the jni hook call back pointers.
	hookObj = (*env)->NewGlobalRef(env, obj);
	jclass cls = (*env)->GetObjectClass(env, hookObj);
	fireKeyPressed_ID = (*env)->GetMethodID(env, cls, "fireKeyPressed", "(JIIC)V");
	fireKeyReleased_ID = (*env)->GetMethodID(env, cls, "fireKeyReleased", "(JIIC)V");

	//Call listener
	hookThreadId = pthread_self();

	#ifdef DEBUG
	printf("Native: MsgLoop() start successful.\n");
	#endif

	MsgLoop();
}

JNIEXPORT void JNICALL Java_org_jnativegrab_keyboard_GrabKeyHook_stopHook(JNIEnv * env, jobject obj) {
	bRunning = FALSE;

	pthread_cancel(hookThreadId);
	#ifdef DEBUG
	printf("Native: pthread_cancel successful.\n");
	#endif

	#ifdef DEBUG
	printf("Native: MsgLoop() stop successful.\n");
	#endif
}

JNIEXPORT void JNICALL Java_org_jnativegrab_keyboard_GrabKeyHook_stopHook(JNIEnv * env, jobject obj, jobject ) {

}

void Init() {
	#ifdef DEBUG
	printf("Native: Init - Shared Object Process Attach.\n");
	#endif

	JNIEnv * env = NULL;
}

void Cleanup() {
	//Do Nothing

	#ifdef DEBUG
	printf("Native: Init - Shared Object Process Detach.\n");
	#endif
}

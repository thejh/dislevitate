#define LINUX
#define SGX530
#define USE_SGX_CORE_REV_HEAD

#include <stddef.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <img_defs.h>
#include <services.h>
#include <pvr_bridge_km.h>
#include <pvr_debug.h>
#include <ra.h>
#include <pvr_bridge.h>
#include <sgx_bridge.h>
#include <perproc.h>
#include <device.h>
#include <buffer_manager.h>
#include <pdump_km.h>
#include <syscommon.h>
#include <bridged_pvr_bridge.h>
#include <bridged_sgx_bridge.h>
#include <env_data.h>
#include <mmap.h>
#include <srvkm.h>

// max distance: 0x02000000
unsigned int make_linked_jump_op(unsigned int src, unsigned int dst) {
  unsigned int o;
  int distance;
  
  distance = (int)(   ((long long)dst) - (((long long)src) + 8)   );
  if (distance > 32*1024*1024 || distance < -32*1024*1024) {
    printk(KERN_ERR "distance too big!\n");
    return 0; // crash, BOOOOM!
  }
  distance = distance / 4; /* read: ">>2" */
  o = *((unsigned int *)(&distance)); // is there a proper way to do this, too?
  o = (o & 0x00ffffff) + 0xeb000000;
  return o;
}

char hexchar(char c) {
  return (c < 10) ? ('0' + c) : ('a' + (c - 10));
}

char *hexify(char *ptr, int len) {
  int i;
  char *hexdata;
  hexdata = kmalloc(len*2+1, GFP_ATOMIC/* i'm a kernel newbie and don't know what's right here - therefore, just use something atomic */);
  if (hexdata == NULL) return NULL;
  for (i=0; i<len; i++) {
    hexdata[i*2+0] = hexchar(*(ptr+i) >> 4  ); // high part
    hexdata[i*2+1] = hexchar(*(ptr+i) & 0x0f); // low part
  }
  hexdata[len*2] = '\0';
  return hexdata;
}

int BridgedDispatchKM_FIXCODE(PVRSRV_PER_PROCESS_DATA *psPerProc, PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM) {
  int (*orig_fn)(PVRSRV_PER_PROCESS_DATA *psPerProc, PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM);
  if (psBridgePackageKM->ui32InBufferSize > PVRSRV_MAX_BRIDGE_IN_SIZE) {
    printk(KERN_ALERT "LEVITATOR EXPLOIT DETECTED! Someone tried to write beyond the allowed boundaries. Evil PID is %d.\n", current->pid);
    ReleaseHandleBatch(psPerProc);
    return -EFAULT;
  }
  if (psBridgePackageKM->ui32OutBufferSize > PVRSRV_MAX_BRIDGE_OUT_SIZE) {
    printk(KERN_ALERT "LEVITATOR EXPLOIT DETECTED! Someone tried to read beyond the allowed boundaries. Evil PID is %d.\n", current->pid);
    ReleaseHandleBatch(psPerProc);
    return -EFAULT;
  }
  orig_fn = (void *) 0xc01e0640;
  return orig_fn(psPerProc, psBridgePackageKM);
}

int init_module(void) {
  //char *hex_code;
  
  /* interesting code position where we want to patch, */
  unsigned int *origcall;
  /* code we want to find there... */
  unsigned int call_origfunc_from_origcaller_code;
  /* ...and code we want to replace it with */
  unsigned int call_myfunc_code;
  
  
  printk(KERN_INFO "Hello android kernel... going to patch the levitator security hole, please wait...\n");
  // c01d5f78-c01d6168: PVRSRV_BridgeDispatchKM (caller) (call at c01d6088)
  // c01e0640-c01e076c: BridgedDispatchKM       (callee)
  
  /*
  hex_code = hexify((char *) 0xc01d5f78, 0x10000);
  if (hex_code == NULL) {
    printk(KERN_INFO "fail.\n");
  } else {
    printk(KERN_INFO "%s\n", hex_code);
    kfree(hex_code);
    printk(KERN_INFO "done.\n");
  }
  */
  
  origcall = (unsigned int *) 0xc01d6088;
  call_origfunc_from_origcaller_code = make_linked_jump_op(0xc01d6088, 0xc01e0640);
  call_myfunc_code = make_linked_jump_op(0xc01d6088, (unsigned int)(void*)BridgedDispatchKM_FIXCODE);
  
  if (*origcall == call_origfunc_from_origcaller_code) {
    printk(KERN_INFO "found correct 4-byte machine code instruction, replacing it...\n");
    preempt_disable(); /* we don't want anyone to run into a half-written instruction, do we? */
    *origcall = call_myfunc_code;
    preempt_enable();
    printk(KERN_INFO "done! you should be protected against the levitator exploit now!\n");
  } else {
    printk(KERN_ERR "wrong instruction encountered, could not patch the levitator vulnerability!\n");
  }
  
  return 0;
}

void cleanup_module(void) {
  /* interesting code position we patched, */
  unsigned int *origcall;
  /* code we originally found there... */
  unsigned int call_origfunc_from_origcaller_code;
  /* ...and code we replaced it with */
  unsigned int call_myfunc_code;
  
  printk(KERN_INFO "Goodbye android kernel... trying to unpatch...\n");
  
  origcall = (unsigned int *) 0xc01d6088;
  call_origfunc_from_origcaller_code = make_linked_jump_op(0xc01d6088, 0xc01e0640);
  call_myfunc_code = make_linked_jump_op(0xc01d6088, (unsigned int)(void*)BridgedDispatchKM_FIXCODE);
  
  if (*origcall == call_myfunc_code) {
    printk(KERN_INFO "found correct 4-byte machine code instruction, switching the code path back...\n");
    preempt_disable(); /* we don't want anyone to run into a half-written instruction, do we? */
    *origcall = call_origfunc_from_origcaller_code;
    preempt_enable();
    printk(KERN_INFO "done! your protection against the levitator exploit should be gone now!\n");
  } else {
    printk(KERN_ERR "wrong instruction encountered, could not UNPATCH the levitator vulnerability!\n");
  }
}

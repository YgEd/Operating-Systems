#include <linux/kernel.h>
#include<linux/syscalls.h>

SYSCALL_DEFINE1(jmeharg1_syscall, char*, word){
    //returns the amount vowls in word
    int len=0;
    char hold[32];
    int i;
    long mods = 0;
    int nbytes;
    
    printk(KERN_ALERT "input string is: %s\n", word);

    if (!word){
        return -1;
    }

    while(*(word+len) != '\0'){
        len++;
    }
    len++;
    
    if (len > 32){
        return -1;
    }

    nbytes = copy_from_user(hold, word, len);
    if (nbytes != 0){
        //copy_from_user failed
        return -1;
    }
    printk(KERN_ALERT "before: %s\n", hold);
    
   
    for (i=0 ; i<len; i++){
        if (*(hold+i) == 'a' || 
            *(hold+i) == 'e' ||
            *(hold+i) == 'i' ||
            *(hold+i) == 'o' ||
            *(hold+i) == 'u' ||
            *(hold+i) == 'y')
            {
                *(hold+i) = 'J';
                mods++;
            }
    }

    printk(KERN_ALERT "after: %s\n", hold);
    nbytes = copy_to_user(word, hold, len);
    if (nbytes != 0){
        //copy_to_user failed
        return -1;
    }

    return mods;
}


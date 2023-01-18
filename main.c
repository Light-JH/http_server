#include <signal.h>
#include <stdio.h>

static volatile int receivedSigInt = 0;

void intHandler(int signal)
{
    receivedSigInt = 1;
}

int main(int argc, char **argv)
{
    // Regist SIGINT handling
    signal(SIGINT, intHandler);

    while (!receivedSigInt)
    {
    }

    printf("Exiting...\n");

    return 0;
}
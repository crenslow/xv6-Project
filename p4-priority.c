/* ============================================================================= *
 * Hey CS333 Students!                                                           *
 * -------------------                                                           *
 *                                                                               *
 * Do not modify anything in this file without the explicit permission of the    *
 * course staff.                                                                 *
 * ============================================================================= */

#include "types.h"
#include "user.h"
#include "param.h"
#include "pdx.h"

// This tests the promotion facility.
//
// It is assumed that setpriority will return the appropriate success/failure codes
void
testPromotion(void)
{
  int pid, prio, newPrio, rc;
  // Test promotion
  printf(1, "\n\nTesting promotion...\n");
  pid = getpid();

  rc = setpriority(pid, 0);

  if (rc != 0) {
    printf(2, "setpriority returned failure code!\n");
    printf(2, "**** TEST FAILED ****\n\n\n");
    return;
  }

  prio = getpriority(pid);

  sleep(TICKS_TO_PROMOTE + 100);

  newPrio = getpriority(pid);

  if (newPrio != prio && newPrio > prio) {
    printf(1, "Promotion has occurred.\n");
    printf(1, "**** TEST PASSES ****\n\n\n");
  } else {
    printf(2, "Either promotion did not occur or an unexpected change in priority happened.\n");
    printf(2, "**** TEST FAILED ****\n\n\n");
  }
}

// Create a process and change the priority
void
checkPriority(void)
{
  int pid;
  int originalPriority;
  int newPriority;
  int rc;

  printf(1, "Testing that process starts at MAXPRIO\n");

  pid = getpid();
  originalPriority = getpriority(pid);

  printf(1, "Priority after program start is %d\n", originalPriority);
  if (originalPriority != MAXPRIO) {
    printf(2, "Process didn't start at MAXPRIO\n");
    printf(2, "**** TEST FAILED ****\n\n\n");
  } else {
    printf(1, "**** TEST PASSED ****\n\n\n");
  }

  // Test that setting the priority
  //  1. Returns an appropriate code on success
  //  2. Actually changes the priority on success
  rc = setpriority(pid, MAXPRIO);
  if (rc != 0) {
    printf(2, "setpriority(%d, %d) failed!\n", pid, MAXPRIO);
    printf(2, "**** TEST FAILED ****\n\n\n");
  } else {
    // verify that the new priority is what it should be:
    newPriority = getpriority(pid);
    if (newPriority != MAXPRIO) {
      printf(2, "setpriority(%d, %d) failed.\n", pid, MAXPRIO);
      printf(2, "New priority is %d, but it should be %d.\n", newPriority, MAXPRIO);
      printf(2, "**** TEST FAILED ****\n\n\n");
    }
  }

  // Test that the priority cannot be set to a negative number:
  printf(1, "Testing that a priority cannot be set to an out of range value.\n");
  printf(1, "  Testing setting priority to a negative number.\n");
  originalPriority = getpriority(pid);
  rc = setpriority(pid, -1);
  printf(1, "  setPriority(%d, -1) returned %d.\n", pid, rc);
  if (rc != 0) {
    printf(1, "  **** TEST PASSED ****\n\n\n");
  } else {
    printf(2, "  setPriority should have indicated failed.\n");
    printf(2, "  **** TEST FAILED ****\n\n");

    newPriority = getpriority(pid);
    if (newPriority != originalPriority) {
      printf(2, "  setPriority failed but the priority was changed.\n");
      printf(2, "  Original priority was %d; new priority is %d.\n", originalPriority, newPriority);
      printf(2, "  **** TEST FAILED ****\n\n");
    }
  }

  // Test that the priority on a bogus PID cannot be set
  printf(1, "  Testing that a priority cannot be set on a non-existent PID.\n");
  rc = setpriority(32767, MAXPRIO);
  if (rc != 0) {
    printf(1, "  **** TEST PASSED ****\n\n\n");
  } else {
    printf(2, "  Attempted to set the priority of PID 32767 to %d.\n", MAXPRIO);
    printf(2, "  This should have returned a non-zero value but returned %d.\n", rc);
    printf(2, "  **** TEST FAILED ****\n\n");
  }

  // Test the priority of a known PID:
  pid = 1; // init
  int prio = getpriority(pid);
  printf(1, "Priority for pid %d is %d\n", pid, prio);
  printf(1, "Press C-p to verify.\n");
  sleep(5 * TPS);
}

int
main(int argc, char* argv[])
{
  if (MAXPRIO == 0) {
    printf(1, "MAXPRIO is 0. Change MAXPRIO and try again\n");
    exit();
  }

  checkPriority();

  testPromotion();
  exit();
}

/*
 * ============================================================
 * Program:        EECS 348 Assignment 2 - CEO Email Priority Queue
 *
 * Description:    Implements a MaxHeap-based priority queue (list/array
 *                 backed, built from scratch with no library heap
 *                 modules) to prioritize a CEO's incoming emails.
 *                 Emails are ranked first by sender category
 *                 (Boss > Subordinate > Peer > ImportantPerson >
 *                 OtherPerson) and, within the same category, by
 *                 date (newest first).
 *
 * Inputs:         A text file of commands, provided either as a
 *                 command-line argument or piped via stdin:
 *                   EMAIL <category>,<subject>,<date>  - insert email
 *                   NEXT                                - peek highest-priority email
 *                   READ                                - remove highest-priority email (no output)
 *                   COUNT                               - print number of unread emails
 *                 Usage:
 *                   ./program testfile.txt   (read from a named file), or
 *                   ./program < testfile.txt (read from redirected stdin)
 *
 * Output:         Terminal output for NEXT and COUNT commands, per the
 *                 assignment's specified format. READ produces no output.
 *
 * Collaborators: Mainly Claude with some gemini code
 *
 * 
 *
 * Author:         Nathaniel Lanter
 * Creation date:  9/18/2026
 * Revision date:  9/18/2026
 * Revisions:      [e.g., "Fixed heap sift-down bug", "Added empty-queue guards"]
 * ============================================================
 */
// RUN USING "./email_priority_queue_claude Test_file.txt" IN TERMINAL AT FILE LOCATION

#include <stdio.h>   /* for printf, fprintf, fgets */
#include <stdlib.h>  /* for malloc, realloc, free, exit */
#include <string.h>  /* for strcmp, strncmp, strtok, strncpy, strlen */

/* ------------------------------------------------------------
 * Struct: Email
 * Represents a single email in the CEO's inbox.
 * ------------------------------------------------------------ */
typedef struct {
    char category[32];   /* sender category string, e.g. "Boss" */
    char subject[256];   /* subject line (may contain spaces, no commas) */
    char date[16];       /* date in MM-DD-YYYY format, stored as text */
    int  priority;       /* numeric rank derived from category (higher = more urgent) */
    long dateValue;       /* date converted to YYYYMMDD integer, for fast comparison */
} Email;

/* ------------------------------------------------------------
 * Struct: MaxHeap
 * A list/array-backed max-heap of Email structs. Hand-rolled
 * heap logic; no standard-library heap/priority_queue used.
 * ------------------------------------------------------------ */
typedef struct {
    Email *data;   /* dynamically allocated array backing the heap (the "list") */
    int size;      /* current number of emails stored in the heap */
    int capacity;  /* current allocated length of the data array */
} MaxHeap;

/* ------------------------------------------------------------
 * Function: initHeap
 * Initializes an empty heap with a small starting capacity.
 * Input:  pointer to an uninitialized MaxHeap
 * Output: none (heap fields are set in place)
 * ------------------------------------------------------------ */
void initHeap(MaxHeap *h) {
    h->capacity = 8;                                          /* start with room for 8 emails */
    h->size = 0;                                               /* heap starts empty */
    h->data = (Email *)malloc(sizeof(Email) * h->capacity);    /* allocate the backing array */
    if (h->data == NULL) {                                      /* check malloc did not fail */
        fprintf(stderr, "Error: memory allocation failed.\n");  /* report the failure */
        exit(1);                                                 /* abort: can't continue without memory */
    }
}

/* ------------------------------------------------------------
 * Function: growHeapIfNeeded
 * Doubles the backing array's capacity if it is currently full.
 * Input:  pointer to MaxHeap
 * Output: none (heap's array is reallocated in place if needed)
 * ------------------------------------------------------------ */
void growHeapIfNeeded(MaxHeap *h) {
    if (h->size == h->capacity) {                                        /* array has no free slots left */
        h->capacity *= 2;                                                 /* double the capacity */
        Email *newData = (Email *)realloc(h->data, sizeof(Email) * h->capacity); /* request bigger block */
        if (newData == NULL) {                                             /* check realloc did not fail */
            fprintf(stderr, "Error: memory reallocation failed.\n");       /* report the failure */
            exit(1);                                                        /* abort: can't continue */
        }
        h->data = newData;                                                 /* point heap at the new block */
    }
}

/* ------------------------------------------------------------
 * Function: getPriority
 * Maps a sender-category string to a numeric priority rank.
 * Higher number = higher priority = read sooner.
 * Input:  category string (e.g. "Boss")
 * Output: integer priority rank
 * ------------------------------------------------------------ */
int getPriority(const char *category) {
    if (strcmp(category, "Boss") == 0)            return 5;  /* Boss: read first */
    if (strcmp(category, "Subordinate") == 0)     return 4;  /* Subordinate: read next */
    if (strcmp(category, "Peer") == 0)            return 3;  /* Peer: read next */
    if (strcmp(category, "ImportantPerson") == 0) return 2;  /* ImportantPerson: read next */
    if (strcmp(category, "OtherPerson") == 0)     return 1;  /* OtherPerson: read last */
    return 0;                                                  /* unrecognized category: lowest of all */
}

/* ------------------------------------------------------------
 * Function: getDateValue
 * Converts a "MM-DD-YYYY" string into a YYYYMMDD long integer
 * so two dates can be compared with a simple ">" check.
 * Input:  date string in MM-DD-YYYY format
 * Output: long integer in YYYYMMDD form
 * ------------------------------------------------------------ */
long getDateValue(const char *date) {
    int month, day, year;                             /* holders for the parsed numeric fields */
    sscanf(date, "%d-%d-%d", &month, &day, &year);     /* pull month/day/year out of the string */
    return (long)year * 10000L + (long)month * 100L + (long)day; /* combine into one comparable number */
}

/* ------------------------------------------------------------
 * Function: hasHigherPriority
 * Determines whether email 'a' should come before email 'b'
 * in the max-heap (i.e., a is "greater than" b).
 * Rule: higher category priority wins; ties broken by newer date.
 * Input:  two Email structs
 * Output: 1 if a outranks b, 0 otherwise
 * ------------------------------------------------------------ */
int hasHigherPriority(Email a, Email b) {
    if (a.priority != b.priority) {        /* categories differ */
        return a.priority > b.priority;    /* higher category priority wins outright */
    }
    return a.dateValue > b.dateValue;      /* same category: the newer date wins */
}

/* ------------------------------------------------------------
 * Function: swapEmails
 * Swaps two Email structs in place. Used during heap sift up/down.
 * ------------------------------------------------------------ */
void swapEmails(Email *a, Email *b) {
    Email temp = *a;  /* save a's current contents */
    *a = *b;          /* copy b's contents into a's slot */
    *b = temp;        /* copy saved original a into b's slot */
}

/* ------------------------------------------------------------
 * Function: siftUp
 * Restores the max-heap property by moving the element at
 * 'index' upward until its parent outranks it (or it becomes root).
 * ------------------------------------------------------------ */
void siftUp(MaxHeap *h, int index) {
    while (index > 0) {                                          /* stop once we reach the root */
        int parent = (index - 1) / 2;                             /* array-based heap parent formula */
        if (hasHigherPriority(h->data[index], h->data[parent])) { /* child outranks its parent */
            swapEmails(&h->data[index], &h->data[parent]);         /* move it up one level */
            index = parent;                                        /* keep checking from the new position */
        } else {
            break;                                                  /* heap property satisfied: stop */
        }
    }
}

/* ------------------------------------------------------------
 * Function: siftDown
 * Restores the max-heap property by moving the element at
 * 'index' downward until both children no longer outrank it.
 * ------------------------------------------------------------ */
void siftDown(MaxHeap *h, int index) {
    while (1) {                             /* loop until no swap is needed */
        int left = 2 * index + 1;            /* index of left child */
        int right = 2 * index + 2;           /* index of right child */
        int largest = index;                 /* assume current node outranks both children for now */

        if (left < h->size && hasHigherPriority(h->data[left], h->data[largest])) {
            largest = left;                   /* left child actually outranks current largest */
        }
        if (right < h->size && hasHigherPriority(h->data[right], h->data[largest])) {
            largest = right;                  /* right child actually outranks current largest */
        }
        if (largest == index) {
            break;                             /* neither child outranks this node: heap is valid, stop */
        }
        swapEmails(&h->data[index], &h->data[largest]); /* move the higher-priority child up */
        index = largest;                                 /* continue sifting down from its old slot */
    }
}

/* ------------------------------------------------------------
 * Function: insertEmail
 * Adds a new email to the heap, growing the array if necessary,
 * then restoring heap order via siftUp.
 * ------------------------------------------------------------ */
void insertEmail(MaxHeap *h, Email e) {
    growHeapIfNeeded(h);   /* make sure there is room for one more email */
    h->data[h->size] = e;  /* place the new email at the next free slot (end of array) */
    siftUp(h, h->size);    /* bubble it up to its correct heap position */
    h->size++;             /* one more email is now stored in the heap */
}

/* ------------------------------------------------------------
 * Function: peekMax
 * Returns the highest-priority email without removing it.
 * Caller must check heap isn't empty first.
 * ------------------------------------------------------------ */
Email peekMax(MaxHeap *h) {
    return h->data[0];   /* root of a max-heap is always the highest-priority element */
}

/* ------------------------------------------------------------
 * Function: extractMax
 * Removes and returns the highest-priority email from the heap.
 * Caller must check heap isn't empty first.
 * ------------------------------------------------------------ */
Email extractMax(MaxHeap *h) {
    Email top = h->data[0];         /* save the root email to return later */
    h->size--;                       /* shrink the heap by one */
    h->data[0] = h->data[h->size];   /* move the last element into the now-empty root slot */
    if (h->size > 0) {                /* only need to fix heap order if anything remains */
        siftDown(h, 0);                /* restore heap property starting from the root */
    }
    return top;                       /* hand back the removed (formerly highest-priority) email */
}

/* ------------------------------------------------------------
 * Function: freeHeap
 * Releases the heap's backing array.
 * ------------------------------------------------------------ */
void freeHeap(MaxHeap *h) {
    free(h->data);     /* release the dynamically allocated array */
    h->data = NULL;     /* avoid leaving a dangling pointer */
    h->size = 0;         /* reset size for safety */
    h->capacity = 0;      /* reset capacity for safety */
}

/* ------------------------------------------------------------
 * Function: trimNewline
 * Strips a trailing newline/carriage-return from a line read
 * via fgets, so parsing isn't thrown off by line endings.
 * ------------------------------------------------------------ */
void trimNewline(char *line) {
    size_t len = strlen(line);                                     /* current length of the string */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) { /* trailing newline/CR present */
        line[len - 1] = '\0';                                        /* chop it off */
        len--;                                                        /* update length after chopping */
    }
}

/* ------------------------------------------------------------
 * Function: parseEmailCommand
 * Parses a line of the form:
 *   EMAIL <category>,<subject>,<date>
 * into an Email struct. Assumes the format is well-formed as
 * guaranteed by the assignment spec (subject has no commas).
 * ------------------------------------------------------------ */
void parseEmailCommand(char *line, Email *e) {
    char *rest = line + 6;               /* skip past the literal "EMAIL " prefix (5 chars + space) */

    char *category = strtok(rest, ",");   /* first comma-delimited field: the sender category */
    char *subject  = strtok(NULL, ",");   /* second field: the subject line */
    char *date     = strtok(NULL, ",");   /* third field: the date */

    strncpy(e->category, category, sizeof(e->category) - 1); /* copy category into the struct safely */
    e->category[sizeof(e->category) - 1] = '\0';               /* guarantee null-termination */

    strncpy(e->subject, subject, sizeof(e->subject) - 1);      /* copy subject into the struct safely */
    e->subject[sizeof(e->subject) - 1] = '\0';                  /* guarantee null-termination */

    strncpy(e->date, date, sizeof(e->date) - 1);                /* copy date into the struct safely */
    e->date[sizeof(e->date) - 1] = '\0';                         /* guarantee null-termination */

    e->priority = getPriority(e->category);   /* precompute numeric priority once, at insert time */
    e->dateValue = getDateValue(e->date);       /* precompute comparable date value once, at insert time */
}

/* ------------------------------------------------------------
 * Function: printEmailForNext
 * Prints an email in the exact format the NEXT command requires.
 * ------------------------------------------------------------ */
void printEmailForNext(Email e) {
    printf("Next email:\n");           /* header line required by the NEXT output format */
    printf("Sender: %s\n", e.category); /* print the sender category */
    printf("Subject: %s\n", e.subject); /* print the subject line */
    printf("Date: %s\n", e.date);        /* print the date */
}

/* ------------------------------------------------------------
 * Function: main
 * Reads commands line-by-line and dispatches to the appropriate
 * heap operation until end of input. Accepts input either as a
 * command-line filename argument (argv[1]) or, if no argument is
 * given, from standard input (so shell redirection with '<' still
 * works exactly as before).
 * ------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    MaxHeap heap;          /* the CEO's inbox, represented as a max-heap */
    initHeap(&heap);       /* set up an empty priority queue */

    FILE *input = stdin;   /* default: read commands from standard input */

    if (argc > 1) {                              /* a filename was given on the command line */
        input = fopen(argv[1], "r");              /* try to open that file for reading */
        if (input == NULL) {                       /* the file could not be opened */
            fprintf(stderr, "Error: could not open file '%s'\n", argv[1]); /* report why we're stopping */
            freeHeap(&heap);                          /* clean up before exiting */
            return 1;                                  /* signal failure to the shell */
        }
    }

    char line[512];        /* buffer to hold one line of input at a time */

    while (fgets(line, sizeof(line), input) != NULL) { /* read commands until end of input */
        trimNewline(line);                              /* remove trailing \n or \r\n from the line */

        if (strlen(line) == 0) {  /* line was blank after trimming */
            continue;              /* skip it and move on to the next line */
        }

        if (strncmp(line, "EMAIL", 5) == 0) {
            /* --- EMAIL command: insert a new email into the priority queue --- */
            Email e;                        /* struct to hold the parsed email */
            parseEmailCommand(line, &e);    /* fill in e's fields from the line */
            insertEmail(&heap, e);          /* add e to the heap in its correct priority position */

        } else if (strncmp(line, "NEXT", 4) == 0) {
            /* --- NEXT command: peek at the highest-priority email --- */
            if (heap.size == 0) {                       /* nothing in the inbox */
                printf("There are no emails to read.\n"); /* report the empty case */
            } else {
                printEmailForNext(peekMax(&heap));         /* print the top email without removing it */
            }

        } else if (strncmp(line, "READ", 4) == 0) {
            /* --- READ command: remove the highest-priority email, no output --- */
            if (heap.size > 0) {         /* only remove something if the inbox is not empty */
                extractMax(&heap);        /* remove the top email; its value is intentionally unused */
            }
            /* if the heap is already empty, READ is a safe no-op per the spec's edge cases */

        } else if (strncmp(line, "COUNT", 5) == 0) {
            /* --- COUNT command: report how many unread emails remain --- */
            printf("There are %d emails to read.\n", heap.size); /* print current heap size */

        } else {
            /* unrecognized command: report it but do not crash the program */
            fprintf(stderr, "Warning: unrecognized command '%s'\n", line);
        }
    }

    if (input != stdin) {   /* only close the file if we actually opened one ourselves */
        fclose(input);        /* release the file handle */
    }

    freeHeap(&heap);   /* release the heap's dynamically allocated memory before exiting */
    return 0;           /* signal successful program completion */
}
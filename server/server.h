// Copyright (C) 2008-2009 David Sugar, Tycho Softworks.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "../config.h"
#include <bayonne.h>

#define MAX_DIGITS      48
#define MAX_NAME_LEN    64
#define MAX_TIMESLOTS   960

#define DEBUG1  shell::DEBUG0
#define DEBUG2  (shell::loglevel_t(((unsigned)shell::DEBUG0 + 1)))
#define DEBUG3  (shell::loglevel_t(((unsigned)shell::DEBUG0 + 2)))

#define RTP_BUFFER_SIZE 3

NAMESPACE_BAYONNE
using namespace UCOMMON_NAMESPACE;

class Timeslot;

typedef uint16_t    keymask_t;
typedef uint32_t    sigmask_t;

typedef enum {
    ENTER_STATE = 100,
    EXIT_STATE,
    STOP_STATE,
    EXIT_NOTIFY,
    TIMER_EXIRED,
    TIMER_EXIT,
    TIMER_SYNC,
    SERVICE_SUCCESS,
    SERVICE_FAILURE,
    SERVICE_AUTHORIZE,
    SIGNAL_TEXT,
    SEND_MESSAGE,
    JOIN_TIMESLOTS,
    PART_TIMESLOTS,
    NULL_EVENT,
    CHILD_START,
    CHILD_FAIL,
    CHILD_EXIT,
    JOIN_NOTIFY,
    FAX_EVENT,

    SHELL_EXIT = 200,
    SHELL_START,
    SHELL_WAIT,
    START_SCRIPT,
    RING_START,
    RING_REDIRECT,
    STOP_DISCONNECT,
    SYNC_PARENT,

    ASR_START = 300,
    ASR_TEXT,
    ASR_PARTIAL,
    ASR_VOICE,

    START_INCOMING = RING_START,
    START_OUTGOING = START_SCRIPT,

    MAKE_TEST = 400,
    MAKE_BUSY,
    MAKE_IDLE,
    MAKE_STEP,
    MAKE_STANDBY,
    TEST_IDLE,
    TEST_FAILURE,

    LINE_WINK = 500,
    RINGING_ON,
    RINGING_OFF,
    RINGING_DID,
    ON_HOOK,
    OFF_HOOK,
    CALLER_ID,

    CALL_DETECT = 600,
    CALL_CONNECT,
    CALL_RELEASE,
    CALL_ACCEPT,
    CALL_ANSWERED,
    CALL_HOLD,
    CALL_NOHOLD,
    CALL_DIGITS,
    CALL_OFFER,
    CALL_ANI,
    CALL_ACTIVE,
    CALL_NOACTIVE,
    CALL_BILLING,
    CALL_RESTART,
    CALL_SETSTATE,
    CALL_FAILURE,
    CALL_ALERTING,
    CALL_INFO,
    CALL_BUSY,
    CALL_DIVERT,
    CALL_FACILITY,
    CALL_FRAME,
    CALL_NOTIFY,
    CALL_NSI,
    CALL_RINGING,
    CALL_DISCONNECT,

    DEVICE_OPEN = 700,
    DEVICE_CLOSE,
    DEVICE_BLOCKED,
    DEVICE_UNBLOCKED,

    AUDIO_IDLE = 800,
    AUDIO_BUFFER,
    AUDIO_START,
    AUDIO_STOP,
    INPUT_PENDING,
    OUTPUT_PENDING,
    DTMF_KEYDOWN,
    DTMF_KEYUP,
    VOX_DETECT,
    VOX_SILENCE,
    TONE_START,
    TONE_STOP,
    TONE_IDLE,
    DSP_READY,
    CPA_DIALTONE,
    CPA_BUSYTONE,
    CPA_RINGING,
    CPA_RINGBACK = CPA_RINGING,
    CPA_INTERCEPT,
    CPA_NODIALTONE,
    CPA_NORINGBACK,
    CPA_NOANSWER,
    CPA_CONNECT,
    CPA_FAILURE,
    CPA_GRUNT,
    CPA_REORDER,
    CPA_STOPPED,

    DRIVER_SPECIFIC = 900
} tsevent_t;

/**
 * Speed adjustments for audio.  For timeslot drivers which can do this.
 */
typedef enum {
    SPEED_FAST,
    SPEED_SLOW,
    SPEED_NORMAL
} tsspeed_t;

typedef enum {
    PULSE_DIALER,
    DTMF_DIALER,
    MF_DIALER
} tsdialer_t;

/**
 * This is most often associated with drivers that dynamically allocate dsp
 * resources as needed on demand.  This is used to indicate which dsp resource
 * has currently been allocated for the timeslot.
 */
typedef enum {
    DSP_MODE_INACTIVE = 0,  // dsp is idle
    DSP_MODE_VOICE,     // standard voice processing
    DSP_MODE_CALLERID,  // caller id support
    DSP_MODE_DATA,      // fsk modem mode
    DSP_MODE_FAX,       // fax support
    DSP_MODE_TDM,       // TDM bus with echo cancellation
    DSP_MODE_RTP,       // VoIP full duplex
    DSP_MODE_DUPLEX,    // mixed play/record
    DSP_MODE_JOIN,      // support of joined channels
    DSP_MODE_CONF,      // in conference
    DSP_MODE_TONE       // tone processing
} dspmode_t;

/**
 * Timeslot call mode.  This specifies the line state of the timeslot.
 */
typedef enum {
    TS_UNCONNECTED = 0, // no call activity set yet
    TS_RINGING,         // ringing (incoming or outgoing)...
    TS_CONNECTED,       // timeslot is connected
    TS_HANGUP           // timeslot is disconnecting...
} tsmode_t;

/**
 * This is a special "data" block that is embedded in each channel.  The
 * content of this data block depends on which state the driver timeslot is
 * currently processing (stepped) into.  Hence, different driver states each
 * use different representions of this state data.
 */
typedef union {
    struct {
        unsigned rings;             // rings to answer on
        timeout_t timeout;          // maximum wait time
        const char *transfer;       // id to transfer to if transfering answer
        Timeslot *intercom;         // intercom for transfering answer
        const char *station;        // if answer to fax tone, station id
        const char *fax;            // fax branch script vector
    }   answer;

    struct {
        char pathname[256];
        const char *station;        // fax station id
    }   fax;

    struct {
        union {
            Phrasebook::rule_t rule;
            char path[1024];
        } list;
        Env::pathinfo_t pathinfo;
        unsigned index;             // index into parsed rule list
        unsigned long offset;
        unsigned long limit;
        keymask_t terminate;    // termination keys...
        timeout_t timeout, maxtime;
        unsigned repeat, volume;
        float gain, pitch;
        tsspeed_t speed;
        const char *text;
    } play;

    struct {
        Env::pathinfo_t pathinfo;
        const char *name, *save;
        const char *annotation, *encoding, *text;
        timeout_t timeout;          // max record time
        unsigned long offset;
        keymask_t terminate;
        unsigned long silence, trim, minsize;
        unsigned volume;
        float gain;
        short frames;
        bool append;
        bool info;
        char filepath[128];
        char savepath[128];
    } record;

    struct {
        char digits[MAX_DIGITS + 1];
        char *digit;                // pointer into digits
        const char *callingdigit;
        bool exit;                  // hangup on completion flag
        tsdialer_t dialer;
        timeout_t interdigit;
        timeout_t digittimer;       // for pulse dialing
        timeout_t timeout;
        timeout_t offhook, onhook;  // flash hook timing
        unsigned pulsecount;
    } dialxfer;

    struct {
        timeout_t timeout, first;
        unsigned count;
        keymask_t terminate;
        keymask_t ignore;
        void *map;
        const char *var;
    } collect;

    struct {
        timeout_t wakeup;
        unsigned rings, loops;
        const char *var;
    } sleep;

} tsdata_t;

typedef struct {
    tsevent_t id;

    union {
        struct {
            unsigned digit: 4;          // dtmf digit recieved
            unsigned duration: 12;      // duration of digit
            unsigned e1: 8;             // energy level of tones
            unsigned e2: 8;
        } dtmf;

        struct {
            unsigned tone: 8;
            unsigned energy: 8;         // energy level of tone
            unsigned duration: 16;      // duration of tone
            char *name;                 // name of tone
        } tone;

        struct {
            unsigned digit:  4;         // id if ring cadence supported
            unsigned duration: 24;      // duration of ring
        } ring;

        tsevent_t reason;
        unsigned span, card, tid;
        bool ok;
        int status;
        pid_t pid;
        fd_t fd;
        char dn[8];
        const char *error;
        dspmode_t dsp;
    } parm;
} Event;

class Message : public LinkedObject
{
protected:
    Event event;
    Timeslot *ts;

public:
    Message(Timeslot *timeslot, Event *msg);

    static void deliver(void);
    static void start(int priority);
    static void stop(void);
};

class Group : public LinkedObject
{
protected:
    friend class Driver;

    static Group *groups;
    static Group *spans;

    Mutex lock;
    keydata *keys;
    const char *id, *script;

    volatile unsigned tsUsed;

    unsigned tsFirst;
    unsigned tsCount;
    unsigned span;

    Group(unsigned count);
    Group(keydata *keyset);

public:
    inline bool is_span(void)
        {return span != ((unsigned)(-1));};

    inline bool is_registry(void)
        {return tsFirst == ((unsigned)(-1));};

    inline bool is_mapped(void)
        {return !is_span() && !is_registry() && (tsCount > 0);};

    inline bool is_generic(void)
        {return !is_span() && !is_registry() && (tsCount == 0);};

    inline const char *get(const char *id)
        {return (keys == NULL) ? NULL : keys->get(id);};

    inline const char *get(void)
        {return id;};

    bool assign(Timeslot *ts);
    void release(Timeslot *ts);

    virtual void snapshot(FILE *fp);

    virtual void shutdown(void);

    inline static Group *begin(void)
        {return groups;};
};

class Registry : public Group
{
protected:
    const char *schema;

    Registry(keydata *keyset);

    virtual void snapshot(FILE *fp);

    const char *getHostid(const char *id);

    void getInterface(const char *uri, char *buffer, size_t size);
};

class Driver : public Env, public mempager
{
private:
    friend class Group;

    static Driver *instance;

protected:
    keyfile keyserver, keydriver, keygroup;
    const char *name;
    unsigned tsCount, tsUsed, tsSpan;
    Timeslot **tsIndex;
    char *status;
    keydata *keys;
    Script *definitions;
    timeout_t autotimer;

    volatile unsigned active, down;

    Driver(const char *name, const char *registry = "groups");

    virtual void automatic(void);
    virtual int start(void);
    virtual void stop(void);
    virtual void compile(void);
    virtual Script *load(void);

    keydata *getKeys(const char *groupid);

public:
    inline static Driver *getDriver(void)
        {return instance;};

    inline static timeout_t getTimer(void)
        {return instance->autotimer;};

    inline static const char *getName(void)
        {return instance->name;};

    inline static const char *getStatus(void)
        {return instance->status;};

    inline static unsigned getCount(void)
        {return instance->tsCount;};

    inline static unsigned getUsed(void)
        {return instance->tsUsed;};

    inline static void sync(void)
        {instance->automatic();};

    static Group *getGroup(const char *id);

    static Group *getSpan(unsigned id);

    static Group *getSpan(const char *id);

    static Timeslot *access(unsigned index);

    static Timeslot *request(Group *group = NULL);

    static void release(Timeslot *timeslot);

    static keydata *getPaths(void);

    static keydata *getSystem(void);

    static Script *getImage(void);

    static void release(Script *image);

    static int startup(void);

    static void shutdown(void);

    static void reload(void);

    static void snapshot(void);

    static const char *dup(const char *text);

    static void *alloc(size_t size);
};

class Timeslot : protected OrderedObject, protected Script::interp, protected Mutex
{
protected:
    friend class Message;
    friend class Driver;
    friend class Group;

    typedef bool (Timeslot::*handler_t)(Event *event);

    Group *group;               // active group of current call
    Group *incoming;            // incoming group to answer as...
    Group *span;                // span entity associated with timeslot
    unsigned tsid;
    bool inUse;
    tsmode_t tsmode;
    handler_t handler;
    const char *state;
    long callid;

    Timeslot(Group *group = NULL);

    // generic idle handling
    bool idleHandler(Event *event);

    // set state handler...
    void setHandler(handler_t proc, const char *name, char code);

public:
    virtual unsigned long getIdle(void);        // idle time

    virtual void startup(void);

    virtual void shutdown(void);

    bool post(Event *event);

    inline bool exec(Event *event)
        {return (this->*handler)(event);};

    inline void notify(Event *event)
        {new Message(this, event);};

    static Timeslot *request(long cid);

    static bool request(Timeslot *ts, long cid);

    static void release(Timeslot *ts);

    static Timeslot *get(long cid);

    static unsigned available(void);
};

class RTPTimeslot : public Timeslot, public JoinableThread
{
protected:
    struct sockaddr_storage rtp_contact;
    socklen_t rtp_addrlen;
    char rtp_sending[12 + 480];         // sending header, recast internally
    char rtp_rfc2833[12 + 24];          // for sending rfc2833 packets...
    char rtp_receive[RTP_BUFFER_SIZE][480 + 72];
    socket_t rtp, rtcp;
    timeout_t rtp_slice;
    long rtp_samples;
    int rtp_family, rtp_priority;
    unsigned rtp_port;
    const char *rtp_address;
    uint32_t prior_rfc2833;             // last dtmf timestamp....

    RTPTimeslot(const char *address, unsigned short port, int family);

    void send(void *address, size_t len);

    void create2833(unsigned event);    // create 2833 event...

    void send2833(bool end = false);    // send 2833 event...

    void run(void);

    void startup(void);

    void shutdown(void);
};

class Scheduler : public LinkedObject
{
protected:
    Scheduler(LinkedObject **list);

public:
    const char *group, *event, *script;
    unsigned year, month, day, start, end;
    bool dow[8];

    static const char *select(Script *image, const char *group, const char *event);
    static void load(Script *image, const char *path);
};

class server
{
public:
    static char *status;
    static void start(int argc, char **argv, shell::mainproc_t svc = NULL);
};

#ifdef HAVE_SIGWAIT

class signals : private JoinableThread
{
private:
    bool shutdown;
    bool started;

    sigset_t sigs;

    void run(void);
    void cancel(void);

    signals();
    ~signals();

    static signals thread;

public:
    static void service(const char *name);
    static void setup(void);
    static void start(void);
    static void stop(void);
};

#else
class signals
{
public:
    static void service(const char *name);
    static void setup(void);
    static void start(void);
    static void stop(void);
};
#endif

#ifdef  HAVE_SYS_INOTIFY_H

class __LOCAL notify : private JoinableThread
{
private:
    notify();

    ~notify();

    void run(void);

    static notify thread;

public:
    static void start(void);
    static void stop(void);
};

#else

class __LOCAL notify
{
public:
    static void start(void);
    static void stop(void);
};

#endif



END_NAMESPACE

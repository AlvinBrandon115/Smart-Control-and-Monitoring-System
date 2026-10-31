#pragma once
#include "app_types.h"
#include "app_config.h"

typedef struct { char uid[15]; char name[24]; bool admin; bool inside; time_t since; } user_t;

void state_init(void);                       /* creates mutex, loads users (TODO: from users.csv) */
bool user_lookup(const char *uid, user_t *out);
bool state_enter(const char *uid, time_t ts);  /* atomic: false if unknown or already inside (anti-passback) */
bool state_leave(const char *uid);             /* atomic: false if unknown or not inside */
int  state_count_inside(void);
void state_print_inside(void);

#define DIRTY_USERS  1
#define DIRTY_CONFIG 2
uint8_t state_take_dirty(void);                 /* returns and clears the "needs saving" flags */
void    state_mark_dirty(uint8_t flags);
int     state_users_snapshot(user_t *out, int max);
void    state_users_replace(const user_t *in, int n);
bool    state_user_add(const char *uid, const char *name, bool admin);
bool    state_user_remove(const char *uid);
void    state_print_users(void);

void schedule_set(int open_min, int close_min);
void schedule_get(int *open_min, int *close_min);
bool schedule_allows(time_t now);             /* true if access is allowed at this time */
void override_set(bool on);
bool override_get(void);

typedef enum { T_ENTRY, T_EXIT, T_ROOM1, T_ROOM2, T_LOGGER, T_CLOCK, T_COUNT } terminal_t;
void heartbeat_beat(terminal_t t);          /* each terminal calls this regularly to show it is alive */
bool terminal_online(terminal_t t);         /* true if it beat within the last 5 s */
const char *terminal_name(terminal_t t);

void emergency_set(bool on);                /* emergency exit override */
bool emergency_get(void);

void state_set_last(const char *text);          /* last access result, shown on the LCD */
void state_get_last(char out[17]);

void room_state_set(uint8_t room, bool occupied);
bool room_state_get(uint8_t room);

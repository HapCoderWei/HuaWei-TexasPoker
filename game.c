#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SIZE       1024
#define ERROR      -1
#define BUTTON     10
#define SMALL_BIND 20
#define BIG_BIND   30
#define OTHERS     40

/* Define Macro for round state */
#define HOLD_CARDS_STAT  50
#define FLOP_STAT        60
#define TURN_STAT        70
#define RIVER_STAT       80

/* Define Macro for action */
#define CHECK    0
#define CALL     1
#define RAISE    2
#define ALL_IN   3
#define FOLD     4

/* Define Macro of Card Type */
#define NONE                   0

#define DRAWING_HAND_FLUSH     90
#define DRAWING_HAND_STRAIGHT  95
#define DRAWING_HAND_THREE_KIND 100
#define DRAWING_HAND_TWO_PAIRS  105
#define DRAWING_HAND_PAIR       110

#define MADEHAND_FLUSH  120
#define MADEHAND_STRAIGHT 125
#define MADEHAND_THREE_KIND 130
#define MADEHAND_FOUR_KIND  135
#define MADEHAND_FULL_HOUSE 140

typedef struct SeatInfo {
    int seat;
    unsigned int pid;
    unsigned int jetton;
    unsigned int money;
} SeatInfo;
typedef struct Card {
    char color[10];
    char point;
} Card;
typedef struct HoldCards {
    Card hold_card_one;
    Card hold_card_two;
} HoldCards;
typedef struct FlopCards {
    Card flop_card_one;
    Card flop_card_two;
    Card flop_card_three;
} FlopCards;
typedef struct Blind {
    int big;
    int small;
} Blind;
typedef struct InquireMsg {
    int pid;
    int jetton;
    int money;
    int bet;
    char action[11];
} InquireMsg;
static char *all_info[] = {
        "seat/ \n",
        "blind/ \n",
        "hold/ \n",
        "inquire/ \n",
        "flop/ \n",
        "turn/ \n",
        "river/ \n",
        "showdown/ \n",
        "pot-win/ \n",
        "game-over \n"
    };
static unsigned char action[5][11] = {
    "check \n",
    "call \n",
    "raise ",
    "all_in \n",
    "fold \n"
};
/*                             Flush  Straight  FullHouse, Four, Three*/
static double raise_rate[5] = {3.205, 2.065,    3.752,     59.4, 1.97};
double GetPotEquity(int pot_value);
int CanCheck();
int JudgeCardType_Five(HoldCards hold, FlopCards flop, int *draws);
int DrawingHand(int card_type, double pot_equity);
int MadeHand(int card_type);
int Action_Flop(HoldCards hold, FlopCards flop, double pot_equity, int *raise_num);
int JudgeCardType_Six(HoldCards hold, FlopCards flop, Card turn, int *draws);
int DrawingHand_turn(int card_type, double pot_equity);
int Action_Turn(HoldCards hold, FlopCards flop, Card turn, double pot_equity);
int JudgeCardType_Seven(HoldCards hold, FlopCards flop, Card turn, Card river, int *draws);
int Action_River(HoldCards hold, FlopCards flop, Card turn, Card river);

int Action_HoldCards(HoldCards hold, Blind blind, int preBet);

SeatInfo GetSeatInfo(char sbuf[], int my_id);
int GetPreBet();
int GetBlindMsg(char sbuf[], SeatInfo my_seat, Blind *blind);
void GetHoldCards(char sbuf[], HoldCards *my_hold_cards);
void GetInquire(char sbuf[], InquireMsg inquireMsg[], int *pot_value);
void GetFlopMsg(char buf[], FlopCards *flopcards);
void GetTurnMsg(char buf[], Card *turn);
void GetRiverMsg(char buf[], Card *river);
void GetShowdownMsg(char buf[]);
void GetPotWinMsg(char buf[]);
void GameOverMsg(char buf[]);
void PrintSeatInfo(SeatInfo my_seat);

int state = 0;
InquireMsg inquireMsg[8] = {0};

int main(int argc, char *argv[])
{
    /* argc is 6 */
    /* argv[1] is server ip. eg:"192.168.0.1" */
    /* argv[2] is server port. eg:"1024" */
    /* argv[3] is client ip. eg:"192.168.0.2" */
    /* argv[4] is client port. eg:"2048" */
    /* argv[5] is client id. eg:"6001" */

    int sockfd, client_id;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    int act, i, pot_value, raise_num, preBet;
    char if_raise[20] = "";
    char ch_raise[10] = "";
    double pot_equity;
    SeatInfo my_seat;           /* Information of my seat */
    //InquireMsg inquireMsg[8];   /* Save Inquire Msg */
    Blind blind;                /* Blind value */
    HoldCards my_hold_cards;    /* My hold cards  */
    FlopCards flop_cards;       /* Flop cards */
    Card turn_card;             /* Turn card */
    Card river_card;            /* River card */

    unsigned char reg_msg[50] = "reg: ";
    /* unsigned char seat_info_msg[512] = ""; */
    /* unsigned char blind_msg[254] = ""; */
    /* unsigned char hold_cards_msg[SIZE] = ""; */
    unsigned char sbuf[SIZE] = "";

    char *pinfo[11] = {NULL};

    if(argc < 6) {              /* Check parameters */
        printf("Usage : %s <Server IP> <Server Port> <Client IP> <Client Port> <Client ID>", argv[0]);
        exit(0);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* Create a file discriper */
        perror("Create stream socket");
        exit(1);
    }

    /* Bind local port and ip */
    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(inet_network(argv[3])); /* Client IP */
    client_addr.sin_port = htons(strtol(argv[4],0,0));
    bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr));

    /* Connect to server */
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(inet_network(argv[1])); /* Connect Server IP */
    server_addr.sin_port = htons(strtol(argv[2],0,0));                      /* Connect Server Port */

    while(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0); /* Connecting... */

    /* Start to Register */
    client_id = atoi(argv[5]);
    strcat(reg_msg, argv[5]);
    strcat(reg_msg, " hello \n"); /* Set Register Message */
    write(sockfd, reg_msg, strlen(reg_msg)+1);

    for(;;) {                   /* Join in Game */
        read(sockfd, sbuf, SIZE);

        for (i = 0; i < 10; i++) {
            pinfo[i] = strstr(sbuf, all_info[i]);
        }

        if(NULL != pinfo[4]) {  /* Flop msg */
            GetFlopMsg(pinfo[4], &flop_cards);
            state = FLOP_STAT;
            pinfo[4] = NULL;
        }
        if(NULL != pinfo[3]) {  /* Inquire msg */
            GetInquire(pinfo[3], inquireMsg, &pot_value);
            if(HOLD_CARDS_STAT == state) {
                preBet = GetPreBet();
                act = Action_HoldCards(my_hold_cards, blind, preBet);
                if(act == RAISE) {
                    strcpy(if_raise, "raise");
                    strcat(if_raise, " 100 \n");
                    write(sockfd, if_raise, strlen(if_raise)+1);
                }
                else {
                    write(sockfd, action[act], strlen(action[act])+1);
                }
            }
            else if(FLOP_STAT == state) {
                pot_equity = GetPotEquity(pot_value);
                act = Action_Flop(my_hold_cards, flop_cards, pot_equity, &raise_num);
                if(act == RAISE) {
                    strcpy(if_raise, "raise ");
                    sprintf(ch_raise, "%d", raise_num);
                    strcat(if_raise, ch_raise);
                    strcat(if_raise, " \n");
                    write(sockfd, if_raise, strlen(if_raise) + 1);
                }
                else {
                    write(sockfd, action[act], strlen(action[act])+1);
                }
            }
            else if(TURN_STAT == state) {
                pot_equity = GetPotEquity(pot_value);
                act = Action_Turn(my_hold_cards, flop_cards, turn_card, pot_equity);
                write(sockfd, action[act], strlen(action[act])+1);
            }
            else if(RIVER_STAT == state) {
                act = Action_River(my_hold_cards, flop_cards, turn_card, river_card);
                write(sockfd, action[act], strlen(action[act])+1);
            }
            //write(sockfd, action[i], strlen(action[i])+1);
            pinfo[3] = NULL;
        }

        if(NULL != pinfo[5]) {  /* Turn msg */
            GetTurnMsg(pinfo[5], &turn_card);
            state = TURN_STAT;
            pinfo[5] = NULL;
        }
        if(NULL != pinfo[6]) {  /* River msg */
            GetRiverMsg(pinfo[6], &river_card);
            state = RIVER_STAT;
            pinfo[6] = NULL;
        }
        if(NULL != pinfo[7]) {  /* Showdown msg */
            GetShowdownMsg(pinfo[7]);
            pinfo[7] = NULL;
        }
        if(NULL != pinfo[8]) {  /* Pot win msg */
            GetPotWinMsg(pinfo[8]);
            pinfo[8] = NULL;
        }
        if(NULL != pinfo[9]) {  /* Game Over */
            GameOverMsg(pinfo[9]);
            goto end;           /* break; also Good */
            pinfo[9] = NULL;
        }
        if(NULL != pinfo[0]) {  /* Analysis Seat information */
            my_seat = GetSeatInfo(pinfo[0], client_id);
            pinfo[0] = NULL;
        }
        if(NULL != pinfo[1]) {  /* Force blind-msg */
            GetBlindMsg(pinfo[1], my_seat, &blind);
            //printf("my blind is %d\n", GetBlindMsg(pinfo[1], my_seat), &blind);
            pinfo[1] = NULL;
        }
        if(NULL != pinfo[2]) {  /* hold cards msg */
            GetHoldCards(pinfo[2], &my_hold_cards);
            state = HOLD_CARDS_STAT;
            pinfo[2] = NULL;
        }

        bzero(sbuf, sizeof(sbuf));
    }
end:    close(sockfd);

    exit(0);
}
/* ------------------------------------------- */
/* Need hold cards, blind, previous bet, flop cards, state,  */
int Action_HoldCards(HoldCards hold, Blind blind, int preBet)
{
    char myCard[3] = "";
    myCard[0] = hold.hold_card_one.point;
    myCard[1] = hold.hold_card_two.point;

    if(0 == strcmp(myCard, "AA")) {
        return RAISE;
    }
    else if(0 == strcmp(myCard, "KK") ||
        0 == strcmp(myCard, "QQ")) {
        if(preBet < 5 * blind.big) {
            return CALL;
        }
        else {
            return FOLD;
        }
    }
    else if(0 == strcmp(myCard, "JJ") ||
        (0 == strcmp(myCard, "TT")) || /* 10 & 10 */
        (0 == strcmp(myCard, "AK")) ||
        (0 == strcmp(myCard, "KA")) ||
        (0 == strcmp(myCard, "AQ")) ||
        (0 == strcmp(myCard, "QA")) ||
        (0 == strcmp(myCard, "KQ")) ||
        (0 == strcmp(myCard, "QK")) ||
        (0 == strcmp(myCard, "KJ")) ||
        (0 == strcmp(myCard, "JK")) ||
        (0 == strcmp(myCard, "QJ")) ||
        (0 == strcmp(myCard, "JQ"))) {
        if(preBet < 3 * blind.big) {
            return CALL;
        }
        else {
            return FOLD;
        }
    }
    else if((hold.hold_card_one.color == hold.hold_card_two.color) ||
        (hold.hold_card_one.point == hold.hold_card_two.point)) {
        if(preBet < blind.big) {
            return CALL;
        }
        else {
            return FOLD;
        }
    }
    else {
        return FOLD;
    }
}
double GetPotEquity(int pot_value)
{
    int ready_bet = 0;
    int i;

    for(i = 0; i < 8; ++i) {
        if(0 == strcmp(inquireMsg[i].action, "fold")) { continue; }
        ready_bet = inquireMsg[i].bet;
        break;
    }
    return (pot_value + ready_bet) / ready_bet;
}
int CanCheck()
{
    int i, j;
    char *content[5] = {NULL};

    if(state == HOLD_CARDS_STAT) {
        return 0;
    }
    else {
        for (i = 0; i < 8; ++i) {
            if(0 == strcmp(inquireMsg[i].action, "fold")) { continue; }
            else if(0 == strcmp(inquireMsg[i].action, "check")) { return 1; } /* return CHECK */
            else { return 0; }  /* return FOLD */
        }
    }
}
int JudgeCardType_Five(HoldCards hold, FlopCards flop, int *draws)
{
    int poker[13] = {0};
    int straight[5] = {0};
    int color[4] = {0};
    Card card[5];
    int i, j, t, sum = 0, pairFlag = 0;
    /* card[0].color = hold.hold_card_one.color; */
    /* card[0].point = hold.hold_card_one.point; */
    /* card[1].color = hold.hold_card_two.color; */
    /* card[1].point = hold.hold_card_two.point; */
    /* card[2].color = flop.flop_card_one.color; */
    /* card[2].point = flop.flop_card_one.point; */
    /* card[3].color = flop.flop_card_two.color; */
    /* card[3].point = flop.flop_card_two.point; */
    /* card[4].color = flop.flop_card_three.color; */
    /* card[4].point = flop.flop_card_three.point; */
    card[0] = hold.hold_card_one;
    card[1] = hold.hold_card_two;
    card[2] = flop.flop_card_one;
    card[3] = flop.flop_card_two;
    card[4] = flop.flop_card_three;

    /* Judge Is Flush */
    for (i = 0; i < 5; ++i) {
        if(0 == strcmp(card[i].color, "SPADES")) { color[0]++; }
        else if(0 == strcmp(card[i].color, "HEARTS")) { color[1]++; }
        else if(0 == strcmp(card[i].color, "CLUBS")) { color[2]++; }
        else if(0 == strcmp(card[i].color, "DIAMONDS")) { color[3]++; }
    }
    if(color[0] == 5 || color[1] == 5 || color[2] ==  5 || color[3] == 5) {
        /* Flush!!! God! Damn it!! */
        *draws = 2;
        return MADEHAND_FLUSH;
    }
    if(((color[0] == 0 && color[1] == 0) && (color[2] == 1 || color[3] == 1)) ||
       ((color[0] == 0 && color[2] == 0) && (color[1] == 1 || color[3] == 1)) ||
       ((color[0] == 0 && color[3] == 0) && (color[1] == 1 || color[2] == 1)) ||
       ((color[1] == 0 && color[2] == 0) && (color[0] == 1 || color[3] == 1)) ||
       ((color[1] == 0 && color[3] == 0) && (color[0] == 1 || color[2] == 1)) ||
        ((color[2] == 0 && color[3] == 0) && (color[0] == 1 || color[1] == 1))) {
        /* This situation is A flush draw */
        *draws = 1;
        return DRAWING_HAND_FLUSH;
    }

    /* Judge Is Straight */
    for (i = 0; i < 5; ++i) {
        if(card[i].point == 'A') { straight[i] = 1; }
        else if(card[i].point == 'J') { straight[i] = 11; }
        else if(card[i].point == 'Q') { straight[i] = 12; }
        else if(card[i].point == 'K') { straight[i] = 13; }
        else { straight[i] = card[i].point - '0'; }
        sum += straight[i];
    }
    /* Sort cards */
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4-i; j++) {
            if(straight[j] > straight[j+1]) {
                t = straight[j]; straight[j] = straight[j+1]; straight[j+1] = t;
            }
        }
    }
    if((straight[3] - straight[2] == 1) && (straight[2] - straight[1] == 1)) {
        if((straight[2] - straight[1] == 1) && (straight[4] - straight[3] == 1)) {
            *draws = 2;
            return MADEHAND_STRAIGHT;
        }
        else if((straight[3] - straight[2] == 1) || (straight[2] - straight[1] == 1)) {
            *draws = 1;
            return DRAWING_HAND_STRAIGHT;
        }
    }

    /* Judge Is Four of A Kind */
    for (i = 0; i < 5; ++i) {
        if(card[i].point == 'A') { poker[0]++; }
        else if(card[i].point == 'J') { poker[10]++; }
        else if(card[i].point == 'Q') { poker[11]++; }
        else if(card[i].point == 'K') { poker[12]++; }
        else { poker[card[i].point - '0' - 1]++; }
    }
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 4) {
            *draws = 2;
            return MADEHAND_FOUR_KIND;
        }
        else if(poker[i] == 3) {
            if(poker[(sum-(i+1)*3)/2] == 1) { /* Also have a pair */
                *draws = 2;
                return MADEHAND_FULL_HOUSE;
            }
            else {
                *draws = 1;
                return DRAWING_HAND_THREE_KIND;
            }
        }
    }
    /* Judge Is Two Pairs */
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 2) {
            pairFlag++;
        }
    }
    if(pairFlag == 2) {
        *draws = 1;
        return DRAWING_HAND_TWO_PAIRS;
    }
    else if(pairFlag == 1) {
        *draws = 1;
        return DRAWING_HAND_PAIR;
    }
    else { *draws = 0; return NONE; }
}
int DrawingHand(int card_type, double pot_equity)
{
    if(card_type == DRAWING_HAND_FLUSH) {
        if((pot_equity * 100) < 155) { /* madehand equity >  pot equity */
                return CALL;
            }
        else {
            if(CanCheck()) {    /* If can check, check */  /* Need Write */
                return CHECK;
            }
            else {              /* If cannot check, fold */
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_STRAIGHT) {
        if((pot_equity * 100) < 104) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_THREE_KIND) {
        if((pot_equity * 100) < 2226) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_TWO_PAIRS) {
        if((pot_equity * 100) < 588) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_PAIR) {
        if((pot_equity * 100) < 1090) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
}
int MadeHand(int card_type)
{
    if(state == FLOP_STAT) {
        return RAISE;
    }
    else {
        return CALL;
    }
}
int Action_Flop(HoldCards hold, FlopCards flop, double pot_equity, int *raise_num)
{
    int draws = 0, card_type = ERROR;           /* draws is if card is draws */
    int if_raise = 0;
    int i;

    card_type = JudgeCardType_Five(hold, flop, &draws);

    if(draws == 0) {
        if(CanCheck()) { return CHECK; }
        else { return FOLD; }
    }
    else if(draws == 1) {
        return DrawingHand(card_type, pot_equity);
    }
    else if(draws == 2) {
        if_raise =  MadeHand(card_type);
        if(if_raise == RAISE) {
            for(i = 0; i < 9; ++i) {
                if(0 == strcmp(inquireMsg[i].action, "fold")) { continue; }
                if(card_type == MADEHAND_FLUSH) { *raise_num = raise_rate[0] * inquireMsg[i].bet; }
                else if(card_type == MADEHAND_STRAIGHT) { *raise_num = raise_rate[1] * inquireMsg[i].bet; }
                else if(card_type == MADEHAND_FULL_HOUSE) { *raise_num = raise_rate[2] * inquireMsg[i].bet; }
                else if(card_type == MADEHAND_FOUR_KIND) { *raise_num = raise_rate[3] * inquireMsg[i].bet; }
                else if(card_type == MADEHAND_THREE_KIND) { *raise_num = raise_rate[4] * inquireMsg[i].bet; }
            }
        }
        return if_raise;
    }
}
int JudgeCardType_Six(HoldCards hold, FlopCards flop, Card turn, int *draws)
{
    int poker[13] = {0};
    int straight[6] = {0};
    int color[4] = {0};
    Card card[6];
    int i, j, t, sum = 0, pairFlag = 0;

    card[0] = hold.hold_card_one;
    card[1] = hold.hold_card_two;
    card[2] = flop.flop_card_one;
    card[3] = flop.flop_card_two;
    card[4] = flop.flop_card_three;
    card[5] = turn;

    /* Judge Is Flush */
    for (i = 0; i < 6; ++i) {
        if(0 == strcmp(card[i].color, "SPADES")) { color[0]++; }
        else if(0 == strcmp(card[i].color, "HEARTS")) { color[1]++; }
        else if(0 == strcmp(card[i].color, "CLUBS")) { color[2]++; }
        else if(0 == strcmp(card[i].color, "DIAMONDS")) { color[3]++; }
    }
    if(color[0] == 5 || color[1] == 5 || color[2] ==  5 || color[3] == 5) {
        /* Flush!!! God! Damn it!! */
        *draws = 2;
        return MADEHAND_FLUSH;
    }
    if(color[0] == 3 || color[1] == 3 || color[2] == 3 || color[3] == 3) {
        /* This situation is A flush draw */
        *draws = 1;
        return DRAWING_HAND_FLUSH;
    }

    /* Judge Is Straight */
    for (i = 0; i < 6; ++i) {
        if(card[i].point == 'A') { straight[i] = 1; }
        else if(card[i].point == 'J') { straight[i] = 11; }
        else if(card[i].point == 'Q') { straight[i] = 12; }
        else if(card[i].point == 'K') { straight[i] = 13; }
        else { straight[i] = card[i].point - '0'; }
        sum += straight[i];
    }
    /* Sort cards */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5-i; j++) {
            if(straight[j] > straight[j+1]) {
                t = straight[j]; straight[j] = straight[j+1]; straight[j+1] = t;
            }
        }
    }
    if((straight[4] - straight[3] == 1) && (straight[3] - straight[2] == 1) && (straight[2] - straight[1] == 1)) {
        if((straight[1] - straight[0] == 1) || (straight[5] - straight[4] == 1)) {
            *draws = 2;
            return MADEHAND_STRAIGHT;
        }
        else {
            *draws = 1;
            return DRAWING_HAND_STRAIGHT;
        }
    }

    /* Judge Is Four of A Kind */
    for (i = 0; i < 6; ++i) {
        if(card[i].point == 'A') { poker[0]++; }
        else if(card[i].point == 'J') { poker[10]++; }
        else if(card[i].point == 'Q') { poker[11]++; }
        else if(card[i].point == 'K') { poker[12]++; }
        else { poker[card[i].point - '0' - 1]++; }
    }
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 4) {
            *draws = 2;
            return MADEHAND_FOUR_KIND;
        }
        else if(poker[i] == 3) {
            for (j = 0; j < 13; ++j) {
                if(poker[j] == 2) {
                    pairFlag++;
                }
            }
            if(pairFlag == 1) { /* Also have a pair */
                *draws = 2;
                return MADEHAND_FULL_HOUSE;
            }
            else {
                *draws = 1;
                return DRAWING_HAND_THREE_KIND;
            }
        }
    }
    /* Judge Is Two Pairs */
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 2) {
            pairFlag++;
        }
    }
    if(pairFlag == 2) {
        *draws = 1;
        return DRAWING_HAND_TWO_PAIRS;
    }
    else { *draws = 0; return NONE; }
}
int DrawingHand_turn(int card_type, double pot_equity)
{
    if(card_type == DRAWING_HAND_FLUSH) {
        if((pot_equity * 100) < 424) { /* madehand equity >  pot equity */
                return CALL;
            }
        else {
            if(CanCheck()) {    /* If can check, check */
                return CHECK;
            }
            else {              /* If cannot check, fold */
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_STRAIGHT) {
        if((pot_equity * 100) < 283) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_THREE_KIND) {
        if((pot_equity * 100) < 4661) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
    else if(card_type == DRAWING_HAND_TWO_PAIRS) {
        if((pot_equity * 100) < 1076) {
            return CALL;
        }
        else {
            if(CanCheck()) {
                return CHECK;
            }
            else {
                return FOLD;
            }
        }
    }
}
int Action_Turn(HoldCards hold, FlopCards flop, Card turn, double pot_equity)
{
    int draws = 0, card_type = ERROR;           /* draws is if card is draws */
    card_type = JudgeCardType_Six(hold, flop, turn, &draws);

    if(draws == 0) {
        if(CanCheck()) { return CHECK; }
        else { return FOLD; }
    }
    else if(draws == 1) {
        return DrawingHand_turn(card_type, pot_equity);
    }
    else if(draws == 2) {
        return MadeHand(card_type);
    }
}
int JudgeCardType_Seven(HoldCards hold, FlopCards flop, Card turn, Card river, int *draws)
{
    int poker[13] = {0};
    int straight[6] = {0};
    int color[4] = {0};
    Card card[7];
    int i, j, t, sum = 0, pairFlag = 0;

    card[0] = hold.hold_card_one;
    card[1] = hold.hold_card_two;
    card[2] = flop.flop_card_one;
    card[3] = flop.flop_card_two;
    card[4] = flop.flop_card_three;
    card[5] = turn;

    /* Judge Is Flush */
    for (i = 0; i < 7; ++i) {
        if(0 == strcmp(card[i].color, "SPADES")) { color[0]++; }
        else if(0 == strcmp(card[i].color, "HEARTS")) { color[1]++; }
        else if(0 == strcmp(card[i].color, "CLUBS")) { color[2]++; }
        else if(0 == strcmp(card[i].color, "DIAMONDS")) { color[3]++; }
    }
    if(color[0] == 5 || color[1] == 5 || color[2] ==  5 || color[3] == 5) {
        /* Flush!!! God! Damn it!! */
        *draws = 2;
        return MADEHAND_FLUSH;
    }
    if(color[0] == 3 || color[1] == 3 || color[2] == 3 || color[3] == 3) {
        /* This situation is A flush draw */
        *draws = 1;
        return DRAWING_HAND_FLUSH;
    }

    /* Judge Is Straight */
    for (i = 0; i < 7; ++i) {
        if(card[i].point == 'A') { straight[i] = 1; }
        else if(card[i].point == 'J') { straight[i] = 11; }
        else if(card[i].point == 'Q') { straight[i] = 12; }
        else if(card[i].point == 'K') { straight[i] = 13; }
        else { straight[i] = card[i].point - '0'; }
        sum += straight[i];
    }
    /* Sort cards */
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 6-i; j++) {
            if(straight[j] > straight[j+1]) {
                t = straight[j]; straight[j] = straight[j+1]; straight[j+1] = t;
            }
        }
    }
    if((straight[4] - straight[3] == 1) && (straight[3] - straight[2] == 1)) {
        if((straight[2] - straight[1] == 1) && (straight[5] - straight[4] == 1)) {
            *draws = 2;
            return MADEHAND_STRAIGHT;
        }
        else if((straight[2] - straight[1] == 1) && (straight[1] - straight[0] == 1)) {
            *draws = 2;
            return MADEHAND_STRAIGHT;
        }
        else if((straight[5] - straight[4] == 1) && (straight[6] - straight[5] == 1)) {
            *draws = 2;
            return MADEHAND_STRAIGHT;
        }
        else if((straight[2] - straight[1] == 1) || (straight[5] - straight[4] == 1)) {
            *draws = 1;
            return DRAWING_HAND_STRAIGHT;
        }
    }

    /* Judge Is Four of A Kind */
    for (i = 0; i < 7; ++i) {
        if(card[i].point == 'A') { poker[0]++; }
        else if(card[i].point == 'J') { poker[10]++; }
        else if(card[i].point == 'Q') { poker[11]++; }
        else if(card[i].point == 'K') { poker[12]++; }
        else { poker[card[i].point - '0' - 1]++; }
    }
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 4) {
            *draws = 2;
            return MADEHAND_FOUR_KIND;
        }
        else if(poker[i] == 3) {
            for (j = 0; j < 13; ++j) {
                if(poker[j] == 2) {
                    pairFlag++;
                }
            }
            if(pairFlag == 1) { /* Also have a pair */
                *draws = 2;
                return MADEHAND_FULL_HOUSE;
            }
            else {
                *draws = 1;
                return DRAWING_HAND_THREE_KIND;
            }
        }
    }
    /* Judge Is Two Pairs */
    for (i = 0; i < 13; ++i) {
        if(poker[i] == 2) {
            pairFlag++;
        }
    }
    if(pairFlag == 2) {
        *draws = 1;
        return DRAWING_HAND_TWO_PAIRS;
    }
    else { *draws = 0; return NONE; }
}
int Action_River(HoldCards hold, FlopCards flop, Card turn, Card river)
{
    int draws = 0, card_type = 0;
    card_type = JudgeCardType_Seven(hold, flop, turn, river, &draws);

    if(draws == 0 || draws == 1) {
        if(CanCheck()) { return CHECK; }
        else { return FOLD; }
    }
    else if(draws == 2) {
        return MadeHand(card_type);
    }
}
/* --------------------------------------------- */
SeatInfo GetSeatInfo(char sbuf[], int my_id)
{
    char *seat_info[50] = {NULL};
    char *player_info[6] = {NULL};
    char copy_sbuf[SIZE] = "";
    int pid, jetton, money;
    SeatInfo my_seat;
    int i, j, k;
    my_seat.seat = ERROR;
    strcpy(copy_sbuf, sbuf);

    seat_info[0] = strtok(copy_sbuf, "\n");
    for (k = 1; (seat_info[k] = strtok(NULL, "\n")); k++);

    if(0 == strcmp(seat_info[0], "seat/ ")) {
        for (i = 1; i < k; i++) {
            player_info[0] = strtok(seat_info[i], " ");
            for (j = 1; (player_info[j] = strtok(NULL, " ")); j++);
            if(0 == strcmp(player_info[0], "button:")) {
                pid = atoi(player_info[1]);
                if(my_id == pid) {
                    /* I am button */
                    /* button: pid jetton money */
                    my_seat.seat = BUTTON;
                    my_seat.pid = my_id;
                    my_seat.jetton = atoi(player_info[2]);
                    my_seat.money = atoi(player_info[3]);
                    return my_seat;
                }
            }
            else if(0 == strcmp(player_info[0], "small")) {
                pid = atoi(player_info[2]);
                if(my_id == pid) {
                    /* I am small bind */
                    /* small blind: pid jetton money */
                    my_seat.seat = SMALL_BIND;
                    my_seat.pid = my_id;
                    my_seat.jetton = atoi(player_info[3]);
                    my_seat.money = atoi(player_info[4]);
                    return my_seat;
                }
            }
            else if(0 == strcmp(player_info[0], "big")) {
                pid = atoi(player_info[2]);
                if(my_id == pid) {
                    /* I am big bind */
                    /* big blind: pid jetton money */
                    my_seat.seat = BIG_BIND;
                    my_seat.pid = my_id;
                    my_seat.jetton = atoi(player_info[3]);
                    my_seat.money = atoi(player_info[4]);
                    return my_seat;
                }
            }
            else if((pid = atoi(player_info[0])) == my_id) {
                /* I am turn */
                /* pid jetton money eol */
                my_seat.seat = OTHERS;
                my_seat.pid = my_id;
                my_seat.jetton = atoi(player_info[1]);
                my_seat.money = atoi(player_info[2]);
                return my_seat;
            }
            bzero(player_info, sizeof(player_info));
        }
    }
    return my_seat;
}

int GetBlindMsg(char sbuf[], SeatInfo my_seat, Blind *blind)
{
    char *blind_info[5] = {NULL};
    char *small_blind[3] = {NULL};
    char *big_blind[3] = {NULL};
    char copy_sbuf[40] = "";
    int my_blind = 0;
    int k;
    strcpy(copy_sbuf, sbuf);

    blind_info[0] = strtok(copy_sbuf, "\n");
    for (k = 1; (blind_info[k] = strtok(NULL, "\n")); k++);

    small_blind[0] = strtok(blind_info[1], " ");
    small_blind[1] = strtok(blind_info[1], " ");

    big_blind[0] = strtok(blind_info[2], " ");
    big_blind[1] = strtok(blind_info[2], " ");

    blind->small = atoi(small_blind[1]);
    blind->big   = atoi(big_blind[1]);

    if(my_seat.seat == SMALL_BIND) {
        my_blind = blind->small;
        return my_blind;
    }
    else if(my_seat.seat == BIG_BIND) {
        my_blind = blind->big;
        return my_blind;
    }
    return my_blind;
}
int GetPreBet()
{
    int i;

    for (i = 0; i < 8; ++i) {
        if(0 == strcmp(inquireMsg[i].action, "fold")) { continue; }
        return inquireMsg[i].bet;
    }
}
void GetHoldCards(char sbuf[], HoldCards *my_hold_cards)
{
    char *hold_cards[5] = {NULL};
    char *content[3] = {NULL};
    char copy_sbuf[50] = "";
    int i, j;
    strcpy(copy_sbuf, sbuf);

    hold_cards[0] = strtok(copy_sbuf, "\n");
    for (i = 1; (hold_cards[i] = strtok(NULL, "\n")); i++);
    /* Get first hold card */
    content[0] = strtok(hold_cards[1], " ");
    content[1] = strtok(NULL, " ");
    strcpy(my_hold_cards->hold_card_one.color, content[0]);
    if(0 == strcmp(content[1], "10")) {
        my_hold_cards->hold_card_one.point = 'T';
    }
    else {
        my_hold_cards->hold_card_one.point = content[1][0];
    }
    /* Get second hold card */
    content[0] = strtok(hold_cards[2], " ");
    content[1] = strtok(NULL, " ");
    strcpy(my_hold_cards->hold_card_two.color, content[0]);
    if(0 == strcmp(content[1], "10")) {
        my_hold_cards->hold_card_two.point = 'T';
    }
    else {
        my_hold_cards->hold_card_two.point = content[1][0];
    }
}
void GetInquire(char sbuf[], InquireMsg inquireMsg[], int *pot_value)
{
    char *inquire_msg[10] = {NULL};
    char *content[6] = {NULL};
    char copy_sbuf[256] = "";
    int i, j, k;
    strcpy(copy_sbuf, sbuf);

    inquire_msg[0] = strtok(copy_sbuf, "\n");
    for (i = 1; (inquire_msg[i] = strtok(NULL, "\n")); i++);

    for (j = 1; j < i-2; ++j) { /* the last info is "total pot" */
        content[0] = strtok(inquire_msg[j], " ");
        for (k = 1; (content[k] = strtok(NULL, " ")); ++k);
        inquireMsg[j].pid     = atoi(content[0]);
        inquireMsg[j].jetton  = atoi(content[1]);
        inquireMsg[j].money   = atoi(content[2]);
        inquireMsg[j].bet     = atoi(content[3]);
        strcpy(inquireMsg[j].action, content[4]);
    }
    content[0] = strtok(inquire_msg[j], " ");
    content[1] = strtok(NULL, " ");
    content[2] = strtok(NULL, " ");
    *pot_value = atoi(content[2]);
}
void GetFlopMsg(char buf[], FlopCards *flopcards)
{
    char copy_buf[SIZE] = "";
    char *flop_msg[6] = {NULL};
    char *content[3] = {NULL};
    int i, j;
    strcpy(copy_buf, buf);

    flop_msg[0] = strtok(copy_buf, "\n");
    for (i = 1; (flop_msg[i] = strtok(NULL, "\n")); i++);
    /* Get first flop card */
    content[0] = strtok(flop_msg[1], " ");
    content[1] = strtok(NULL, " ");
    strcpy(flopcards->flop_card_one.color, content[0]);
    flopcards->flop_card_one.point = content[1][0];
    /* Get second flop card */
    content[0] = strtok(flop_msg[2], " ");
    content[1] = strtok(NULL, " ");
    strcpy(flopcards->flop_card_two.color, content[0]);
    flopcards->flop_card_two.point = content[1][0];
    /* Get three flop card */
    content[0] = strtok(flop_msg[3], " ");
    content[1] = strtok(NULL, " ");
    strcpy(flopcards->flop_card_three.color, content[0]);
    flopcards->flop_card_three.point = content[1][0];
}
void GetTurnMsg(char buf[], Card *turn)
{
    char copy_buf[30] = "";
    char *turn_msg[3] = {NULL};
    char *content[3] = {NULL};
    int i;
    strcpy(copy_buf, buf);

    turn_msg[0] = strtok(copy_buf, "\n");
    for (i = 1; (turn_msg[i] = strtok(NULL, "\n")); i++);

    content[0] = strtok(turn_msg[1], " ");
    content[1] = strtok(NULL, " ");

    strcpy(turn->color, content[0]);
    turn->point = content[1][0];
}
void GetRiverMsg(char buf[], Card *river)
{
    char copy_buf[SIZE] = "";
    char *river_msg[4] = {NULL};
    char *content[3] = {NULL};
    int i, j;
    strcpy(copy_buf, buf);

    river_msg[0] = strtok(copy_buf, "\n");
    for (i = 1; (river_msg[i] = strtok(NULL, "\n")); i++);

    content[0] = strtok(river_msg[1], " ");
    content[1] = strtok(NULL, " ");

    strcpy(river->color, content[0]);
    river->point = content[1][0];
}
void GetShowdownMsg(char buf[])
{
    char copy_buf[512] = "";
    char *showdown_msg[20] = {NULL};
    int i, j;
    strcpy(copy_buf, buf);

    showdown_msg[0] = strtok(copy_buf, "\n");
    for (i = 1; (showdown_msg[i] = strtok(NULL, "\n")); i++);

    printf("\n");
    for (i = 0; i < 16; i++) {
        printf("%s", showdown_msg[i]);
    }
    printf("\n");
}
void GetPotWinMsg(char buf[])
{
    char copy_buf[SIZE] = "";
    char *pot_win_msg[11] = {NULL};
    int i, j;
    strcpy(copy_buf, buf);

    pot_win_msg[0] = strtok(copy_buf, "\n");
    for (i = 1; (pot_win_msg[i] = strtok(NULL, "\n")); i++);

    printf("%s", pot_win_msg[1]);
}
void GameOverMsg(char buf[])
{
}
void PrintSeatInfo(SeatInfo my_seat)
{
    if(ERROR == my_seat.seat) {
        printf("Get Seat Info ERROR\n");
        return ;
    }
    if(BUTTON == my_seat.seat) {
        printf("seat info: button ");
    }
    else if(SMALL_BIND == my_seat.seat) {
        printf("seat info: small blind ");
    }
    else if(BIG_BIND == my_seat.seat) {
        printf("seat info: big blind ");
    }
    else {
        printf("seat info: others ");
    }
    printf("jetton: %d ", my_seat.jetton);
    printf("money: %d\n", my_seat.money);
}
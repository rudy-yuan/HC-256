#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/mediastream.h>

#include <ortp/ortp.h>

#define MAX_RTP_SIZE	1500

struct mstream {
    MSFilter *soundread;
    MSFilter *crypto;
    MSFilter *encoder;
    MSFilter *rtpsend;

    MSTicker *ticker;
};

RtpSession *create_duplex_rtpsession(int locport){
	RtpSession *rtpr;
	rtpr=rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_recv_buf_size(rtpr,MAX_RTP_SIZE);
	rtp_session_set_scheduling_mode(rtpr,0);
	rtp_session_set_blocking_mode(rtpr,0);
	rtp_session_enable_adaptive_jitter_compensation(rtpr,FALSE);
	rtp_session_set_symmetric_rtp(rtpr,TRUE);
	rtp_session_set_local_addr(rtpr,"0.0.0.0",locport);
	rtp_session_signal_connect(rtpr,"timestamp_jump",(RtpCallback)rtp_session_resync,(long)NULL);
	rtp_session_signal_connect(rtpr,"ssrc_changed",(RtpCallback)rtp_session_resync,(long)NULL);
	return rtpr;
}

int main(int argc, char **argv)
{
    ortp_init();
    ortp_set_log_level_mask(ORTP_DEBUG|ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
    ortp_scheduler_init();
    printf("0\n");
    RtpSession *session = create_duplex_rtpsession(33333);
    printf("1\n");
    rtp_session_set_remote_addr(session, "127.0.0.1", 44444);

    ms_init();
    ms_load_plugins("/home/rudy/plugin");

    struct mstream *stream;
    stream = (struct mstream *)malloc(sizeof(struct mstream));

    MSSndCard *sndcard;
    sndcard=ms_snd_card_manager_get_card(ms_snd_card_manager_get(), "PulseAudio: default");

    const MSList *list = ms_snd_card_manager_get_list(ms_snd_card_manager_get());

    while (list != NULL) {
	MSSndCard * card = list->data;
	printf("name:%s\n", card->name);
	printf("id:%s\n", card->id);
	printf("driver:%s\n", card->desc->driver_type);
	list = list->next;
    }
    if(sndcard != NULL)
    {
	stream->soundread = ms_snd_card_create_reader(sndcard);
    }
    else 
    {
	stream->soundread=ms_filter_new(MS_FILE_PLAYER_ID);
	audio_stream_play(stream, "test.wav");
	printf("\n\nread from file\n\n");	
    }

    stream->crypto = ms_filter_new_from_name("Encrypt");
    stream->encoder = ms_filter_create_encoder("SPEEX");
    stream->rtpsend=ms_filter_new(MS_RTP_SEND_ID);
    
    int sr = 8000;
    ms_filter_call_method(stream->soundread,MS_FILTER_SET_SAMPLE_RATE,&sr);
    ms_filter_call_method(stream->encoder,MS_FILTER_SET_SAMPLE_RATE,&sr);
    ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_SET_SESSION, session);

    ms_filter_link(stream->soundread, 0, stream->encoder, 0);
    ms_filter_link(stream->encoder, 0, stream->crypto, 0);
    ms_filter_link(stream->crypto, 0, stream->rtpsend, 0);

    stream->ticker = ms_ticker_new();
    ms_ticker_set_name(stream->ticker, "recv stream");
    ms_ticker_attach(stream->ticker, stream->soundread);
        
    int count = 0;
    while (TRUE){
	ortp_global_stats_display();
	if (session) {
	    printf("Bandwidth usage: download=%f kbits/sec, upload=%f kbits/sec\n",
		   rtp_session_compute_recv_bandwidth(session)/1e3,
		   rtp_session_compute_send_bandwidth(session)/1e3);
	}
	usleep(500000);
    }

    ms_exit();
    return 0;
}

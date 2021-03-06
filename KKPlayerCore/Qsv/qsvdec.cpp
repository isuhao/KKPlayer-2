#include <string.h>
#include <sys/types.h>
#include "../Includeffmpeg.h"
extern "C"{
#include <mfx/mfxvideo.h>
#include "libavutil/common.h"
#include "libavutil/mem.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"

#include "libavcodec/avcodec.h"

///此函数需要导出，需要修改FFmpeg
int av_get_format(AVCodecContext *avctx, const enum AVPixelFormat *fmt);///->ff_get_format
int av_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags);    ///->ff_get_buffer;
}

#define h265H264DecodeHeader 0
//#include "internal.h"
#include "qsv.h"
#include "qsv_internal.h"
#include "qsvdec.h"
#include <assert.h>
int ff_qsv_map_pixfmt(enum AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        return AV_PIX_FMT_NV12;
    default:
        return AVERROR(ENOSYS);
    }
}

mfxStatus ExtendMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize);
mfxStatus InitMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize);

int GetNALULen(unsigned char *src,int srcLen,int *remainLen)
{
	int len=0;
	int Index=0;
	unsigned char* pdata=src;
	while(len<srcLen)
	{
			
		if(
			(
			*pdata==0x00&&
			*(1+pdata)==0x00&&
			*(2+pdata)==0x01
			)
			||
			(
				*pdata==0x00&&
				*(1+pdata)==0x00&&
				*(2+pdata)==0x00&&
				*(3+pdata)==0x01
			)
		)
		{
            break;
		}
		len++;
		pdata++;
	}
	*remainLen=srcLen-len;
	return len;
}
int GetH264SeparatorLen(unsigned char *src,int srcLen)
{
	if(*src!=0x00)
		return 0;
	unsigned char* pdata=src;
		
	if(
		*pdata==0x00&&
		*(1+pdata)==0x00&&
		*(2+pdata)==0x01
		)
	{
		return 3;
	}
	if(
		*pdata==0x00&&
		*(1+pdata)==0x00&&
		*(2+pdata)==0x00&&
		*(3+pdata)==0x01
		)
	{
		return 4;
	}
	return 0;
}
static int qsv_decode_init(AVCodecContext *avctx, KKQSVContext *q, AVPacket *avpkt)
{

	if (!avctx->hwaccel_context){
	    assert(0);
	}
	KK_AVQSVContext *qsv = ( KK_AVQSVContext *)avctx->hwaccel_context;
	q->session        = qsv->session;
    q->iopattern      = qsv->iopattern;
    q->ext_buffers    = qsv->ext_buffers;
    q->nb_ext_buffers = qsv->nb_ext_buffers;
	q->async_depth=     qsv->param.AsyncDepth;
   
    const AVPixFmtDescriptor *desc;
	mfxVideoParam param = { { 0 } };
	mfxBitstream bs   = { { { 0 } } };
	int frame_width  = avctx->coded_width;
    int frame_height = avctx->coded_height;
    int ret;
	uint8_t *dummy_data;
    int dummy_size;
    enum AVPixelFormat pix_fmts[3] = { AV_PIX_FMT_QSV,AV_PIX_FMT_NV12,AV_PIX_FMT_NONE };

	/* desc = av_pix_fmt_desc_get(avctx->sw_pix_fmt);
    if (!desc)
        return AVERROR_BUG;*/
   // ret = av_get_format(avctx, pix_fmts);
	/*ret=;
    if (ret < 0)
        return ret;*/

    avctx->pix_fmt      = (AVPixelFormat)AV_PIX_FMT_QSV;
	
    if (!q->session) {
        assert(0);
    }

	if (avpkt->size) {
        bs.Data       = avpkt->data;
        bs.DataLength = avpkt->size;
        bs.MaxLength  = bs.DataLength;
        bs.TimeStamp  = avpkt->pts;
    } else
        return AVERROR_INVALIDDATA;

    ret = ff_qsv_codec_id_to_mfx(avctx->codec_id);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Unsupported codec_id %08x\n", avctx->codec_id);
        return ret;
    }
   
    param.mfx.CodecId = ret;


	 if (!q->avctx_internal) {
        q->avctx_internal = avcodec_alloc_context3(NULL);
        if (!q->avctx_internal)
            return AVERROR(ENOMEM);

        q->parser = av_parser_init(avctx->codec_id);
        if (!q->parser)
            return AVERROR(ENOMEM);

        q->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
        q->orig_pix_fmt   = AV_PIX_FMT_NONE;
    }

	av_parser_parse2(q->parser, q->avctx_internal,
                     &dummy_data, &dummy_size,
                     avpkt->data, avpkt->size, avpkt->pts, avpkt->dts,
                     avpkt->pos);

	 /* TODO: flush delayed frames on reinit */
    if (q->parser->format       != q->orig_pix_fmt    ||
        q->parser->coded_width  != avctx->coded_width ||
        q->parser->coded_height != avctx->coded_height) {
        
			
	    enum AVPixelFormat pix_fmts[3] = { AV_PIX_FMT_QSV,AV_PIX_FMT_NONE,AV_PIX_FMT_NONE };
        enum AVPixelFormat qsv_format;

        qsv_format = (AVPixelFormat)kk_qsv_map_pixfmt((AVPixelFormat)q->parser->format, &q->fourcc);
        if (qsv_format < 0) {
            av_log(avctx, AV_LOG_ERROR,
                   "Decoding pixel format '%s' is not supported\n",
                   av_get_pix_fmt_name((AVPixelFormat)q->parser->format));
            ret = AVERROR(ENOSYS);
         //   goto reinit_fail;
        }

        q->orig_pix_fmt     = (AVPixelFormat)q->parser->format;
        avctx->pix_fmt      = pix_fmts[1] = qsv_format;
        avctx->width        = q->parser->width;
        avctx->height       = q->parser->height;
        avctx->coded_width  = q->parser->coded_width;
        avctx->coded_height = q->parser->coded_height;
        avctx->field_order  = q->parser->field_order;
        avctx->level        = q->avctx_internal->level;
        avctx->profile      = q->avctx_internal->profile;

        ret =AV_PIX_FMT_QSV;// av_get_format(avctx, pix_fmts);
        /*if (ret < 0)
            goto reinit_fail;*/

        avctx->pix_fmt =(AVPixelFormat) ret;

        //ret = qsv_decode_init(avctx, q);
        //if (ret < 0)
        //    goto reinit_fail;
	}

	if(h265H264DecodeHeader)
	{
		ret = MFXVideoDECODE_DecodeHeader(q->session, &bs, &param);
		if (MFX_ERR_MORE_DATA==ret) {
			/* this code means that header not found so we return packet size to skip
			   a current packet
			 */
			return avpkt->size;
		} else if (ret < 0) {
			av_log(avctx, AV_LOG_ERROR, "Decode header error %d\n", ret);
			return ff_qsv_error(ret);
		}

		
		if(!q->sequence_fifo){
			q->sequence_fifo = av_fifo_alloc(1024*16);
		}
		
		int   RemainLen=bs.DataLength;
		unsigned char *pDataNALU=bs.Data;
		while(RemainLen!=0)
		{
				int SepLen= GetH264SeparatorLen(pDataNALU,4);
				char H264Type=0x00;
		
				pDataNALU=(pDataNALU+SepLen);
				RemainLen=RemainLen-SepLen;
				unsigned char NALUType=*pDataNALU&0x1f;
				unsigned char aud=0;

				int NaluLen=0;
				if(NALUType==0x07)     //sequence parameter sps.
				{
					NaluLen= GetNALULen(pDataNALU,RemainLen,&RemainLen);
					av_fifo_generic_write(q->sequence_fifo, pDataNALU-SepLen,NaluLen+SepLen, NULL);
				}else if(NALUType==0x08)     //picture parameter pps
				{
					NaluLen= GetNALULen(pDataNALU,RemainLen,&RemainLen);
					av_fifo_generic_write(q->sequence_fifo, pDataNALU-SepLen,NaluLen+SepLen, NULL);
				}else{
					NaluLen= GetNALULen(pDataNALU,RemainLen,&RemainLen);
				}
				pDataNALU=(pDataNALU+NaluLen);
		}
	}
   /* mfxStatus sts=MFXVideoDECODE_Query(q->session,&param, &param);
	
	 if (sts < 0) {
		return ff_qsv_error(sts);
	 }*/
	
	param.IOPattern   = q->iopattern;
    param.AsyncDepth  = q->async_depth;
    param.ExtParam    = q->ext_buffers;
    param.NumExtParam = q->nb_ext_buffers;
    param.mfx.FrameInfo.BitDepthLuma   = 8;
    param.mfx.FrameInfo.BitDepthChroma = 8;
	/*param.mfx.FrameInfo.BitDepthLuma   = 8;//desc->comp[0].depth;
    param.mfx.FrameInfo.BitDepthChroma = desc->comp[0].depth;
    param.mfx.FrameInfo.Shift          = desc->comp[0].depth > 8;;*/

    param.mfx.Rotation = 0;
	param.mfx.CodecProfile = kk_qsv_profile_to_mfx(avctx->codec_id,avctx->profile);

	param.mfx.CodecLevel=avctx->level  == FF_LEVEL_UNKNOWN ? MFX_LEVEL_UNKNOWN: avctx->level;
	// int xc=MFX_FOURCC_NV12;
	param.mfx.FrameInfo.FourCC         = q->fourcc;
    param.mfx.FrameInfo.Width          = frame_width;
    param.mfx.FrameInfo.Height         = frame_height;
    param.mfx.FrameInfo.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;

    switch (avctx->field_order) {
    case AV_FIELD_PROGRESSIVE:
        param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    case AV_FIELD_TT:
        param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        break;
    case AV_FIELD_BB:
        param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        break;
    default:
        param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
		//param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    }


	qsv->param=param;

	/*mfxFrameAllocRequest request;
	memset(&request,0,sizeof(request));
	ret=MFXVideoDECODE_QueryIOSurf(q->session, &param,&request);*/
	
	/**/
	//sts=MFXVideoDECODE_Query(decCtx->mfx_session,&decCtx->dec_param, &decCtx->dec_param);
    ret = MFXVideoDECODE_Init(q->session, &param);
    if (ret < 0) {
        if (MFX_ERR_INVALID_VIDEO_PARAM==ret) {
            av_log(avctx, AV_LOG_ERROR,
                   "Error initializing the MFX video decoder, unsupported video\n");
        } else {
            av_log(avctx, AV_LOG_ERROR,
                   "Error initializing the MFX video decoder %d\n", ret);
        }
        return ff_qsv_error(ret);
    }

	

   /* avctx->profile      = param.mfx.CodecProfile;
    avctx->level        = param.mfx.CodecLevel;
    avctx->coded_width  = param.mfx.FrameInfo.Width;
    avctx->coded_height = param.mfx.FrameInfo.Height;
	int w=param.mfx.FrameInfo.CropW - param.mfx.FrameInfo.CropX;
	int h=param.mfx.FrameInfo.CropH - param.mfx.FrameInfo.CropY;
	avctx->width        = w==0 ? avctx->coded_width:w;
	avctx->height       = h==0 ? avctx->coded_height:h;*/

    /* maximum decoder latency should be not exceed max DPB size for h.264 and
       HEVC which is 16 for both cases.
       So weare  pre-allocating fifo big enough for 17 elements:
     */
   

    if (!q->input_fifo) {
        q->input_fifo = av_fifo_alloc(1024*16);
        if (!q->input_fifo)
            return AVERROR(ENOMEM);
    }

    if (!q->pkt_fifo) {
        q->pkt_fifo = av_fifo_alloc( sizeof(AVPacket) * (1 + 16) );
        if (!q->pkt_fifo)
            return AVERROR(ENOMEM);
    }
	
    q->engine_ready = 1;

    return 0;
}

static int alloc_frame(AVCodecContext *avctx, KKQSVFrame *frame)
{
    int ret;

    ret = av_get_buffer(avctx, frame->frame, AV_GET_BUFFER_FLAG_REF);
    if (ret < 0)
        return ret;

    if (frame->frame->format == AV_PIX_FMT_QSV) {
        frame->surface = (mfxFrameSurface1*)frame->frame->data[3];
    } else {
        frame->surface_internal.Info.BitDepthLuma   = 8;
        frame->surface_internal.Info.BitDepthChroma = 8;
        frame->surface_internal.Info.FourCC         = MFX_FOURCC_NV12;
        frame->surface_internal.Info.Width          = avctx->coded_width;
        frame->surface_internal.Info.Height         = avctx->coded_height;
        frame->surface_internal.Info.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;

        frame->surface_internal.Data.PitchLow = frame->frame->linesize[0];
        frame->surface_internal.Data.Y        = frame->frame->data[0];
        frame->surface_internal.Data.UV       = frame->frame->data[1];

        frame->surface = &frame->surface_internal;
    }

    return 0;
}

static void qsv_unlock_used_frames(KKQSVContext *q)
{
   // KKQSVFrame *cur = (KKQSVFrame *)q->work_frames;
   // while (cur) {
   //     if (cur->surface&& !cur->queued) {
			////cur->surface->Data.Locked=0;
   //     }
   //     cur = cur->next;
   // }
}

static void qsv_clear_unused_frames(KKQSVContext *q)
{
    KKQSVFrame *cur = (KKQSVFrame *)q->work_frames;
    while (cur) {
		int *index=(int*)cur->frame->data[5];
        if(cur->surface && !cur->surface->Data.Locked && !cur->queued) {
			if(index!=0)
			   *index=0;
			cur->surface=NULL;
            av_frame_unref(cur->frame);
        }
        cur = cur->next;
    }
}

static int get_surface(AVCodecContext *avctx, KKQSVContext *q, mfxFrameSurface1 **surf)
{
    KKQSVFrame *frame, **last;
    int ret;

   
    qsv_clear_unused_frames(q);
    frame = q->work_frames;
    last  = &q->work_frames;
    while (frame) {
        if (!frame->surface) {
            ret = alloc_frame(avctx, frame);
            if (ret < 0)
                return ret;
            *surf = frame->surface;
            return 0;
        }

        last  = &frame->next;
        frame = frame->next;
    }

    frame =(KKQSVFrame *) av_mallocz(sizeof(*frame));
    if (!frame)
        return AVERROR(ENOMEM);
    frame->frame = av_frame_alloc();
    if (!frame->frame) {
        av_freep(&frame);
        return AVERROR(ENOMEM);
    }
    *last = frame;

    ret = alloc_frame(avctx, frame);
    if (ret < 0)
        return ret;

    *surf = frame->surface;

    return 0;
}

static KKQSVFrame *find_frame(KKQSVContext *q, mfxFrameSurface1 *surf)
{
    KKQSVFrame *cur = q->work_frames;
    while (cur) {
        if (surf == cur->surface)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/*  This function uses for 'smart' releasing of consumed data
    from the input bitstream fifo.
    Since the input fifo mapped to mfxBitstream which does not understand
    a wrapping of data over fifo end, we should also to relocate a possible
    data rest to fifo begin. If rest of data is absent then we just reset fifo's
    pointers to initial positions.
    NOTE the case when fifo does contain unconsumed data is rare and typical
    amount of such data is 1..4 bytes.
*/
static void qsv_fifo_relocate(AVFifoBuffer *f, int bytes_to_free)
{
    int data_size;
    int data_rest = 0;

    av_fifo_drain(f, bytes_to_free);

    data_size = av_fifo_size(f);
    if (data_size > 0) {
        if (f->buffer!=f->rptr) {
            if ( (f->end - f->rptr) < data_size) {
                data_rest = data_size - (f->end - f->rptr);
                data_size-=data_rest;
                memmove(f->buffer+data_size, f->buffer, data_rest);
            }
            memmove(f->buffer, f->rptr, data_size);
            data_size+= data_rest;
        }
    }
    f->rptr = f->buffer;
    f->wptr = f->buffer + data_size;
    f->wndx = data_size;
    f->rndx = 0;
}


static void close_decoder(KKQSVContext *q)
{
    KKQSVFrame *cur;

	if (q->session){
        MFXVideoDECODE_Close(q->session);
		q->session=0;
	}

  

    cur = q->work_frames;
    while (cur) {
        q->work_frames = cur->next;
        av_frame_free(&cur->frame);
        av_freep(&cur);
        cur = q->work_frames;
    }

	av_parser_close(q->parser);
	q->parser=0;
    avcodec_free_context(&q->avctx_internal);
	q->avctx_internal=0;

	//MFXClose(q->session);
    q->session=0;
    q->engine_ready   = 0;
    q->reinit_pending = 0;
}


static int do_qsv_decode(AVCodecContext *avctx, KKQSVContext *q,
                  AVFrame *frame, int *got_frame,
                  AVPacket *avpkt)
{
    KKQSVFrame *out_frame=NULL;
    mfxFrameSurface1 *insurf=NULL;
    mfxFrameSurface1 *outsurf=NULL;
  
    mfxBitstream bs = { { { 0 } } };
    int ret;
    int n_out_frames;
    int buffered = 0;
    int flush    = !avpkt->size || q->reinit_pending;

    if (!q->engine_ready||q->orig_pix_fmt== AV_PIX_FMT_NONE) {
        ret = qsv_decode_init(avctx, q, avpkt);
        if (ret)
            return ret;
    }

    if (!flush) {
        if (av_fifo_size(q->input_fifo)) {
            /* we have got rest of previous packet into buffer */
            if (av_fifo_space(q->input_fifo) < avpkt->size) {
                ret = av_fifo_grow(q->input_fifo, avpkt->size);
                if (ret < 0)
                    return ret;
            }
            av_fifo_generic_write(q->input_fifo, avpkt->data, avpkt->size, NULL);
            bs.Data       = q->input_fifo->rptr;
            bs.DataLength = av_fifo_size(q->input_fifo);
            buffered = 1;
        } else {
            bs.Data       = avpkt->data;
            bs.DataLength = avpkt->size;
        }
        bs.MaxLength  = bs.DataLength;
        bs.TimeStamp  = avpkt->pts;
    }

   

   
        ret = get_surface(avctx, q, &insurf);
        if (ret < 0)
            return ret;
        do {
             ret = MFXVideoDECODE_DecodeFrameAsync(q->session,&bs,insurf, &outsurf, &q->sync);
			
			 if (MFX_WRN_VIDEO_PARAM_CHANGED==ret) {
				 /* TODO: handle here minor sequence header changing */
				 continue;
			 } 
			insurf->Data.Locked=1;
			if(ret==MFX_ERR_MORE_DATA)
			{
			    av_fifo_generic_write(q->input_fifo, avpkt->data,avpkt->size, NULL);
			}
            if (ret != MFX_WRN_DEVICE_BUSY)
                break;
            av_usleep(500);
        } while (1);

        
		//解码成功
        if (ret==MFX_ERR_NONE){
				qsv_fifo_relocate(q->input_fifo, bs.DataOffset);
				ret = MFXVideoCORE_SyncOperation(q->session, q->sync, 1000);
				if (MFX_ERR_NONE == ret) { 
					 
							KKQSVFrame *out_frame = find_frame(q, outsurf);
							if (!out_frame) {
								av_log(avctx, AV_LOG_ERROR,
									   "The returned surface does not correspond to any frame\n");
								return AVERROR_BUG;
							}
							AVFrame *src_frame = out_frame->frame;

							ret = av_frame_ref(frame, src_frame);
							if (ret < 0)
								return ret;
							outsurf = out_frame->surface;

							
						  //  WriteNextFrameI420(out_frame->surface);
							frame->data[0]=out_frame->surface->Data.Y;
							frame->data[1]=out_frame->surface->Data.UV;
                            frame->data[4]=(uint8_t *)out_frame->surface;
							frame->format=AV_PIX_FMT_NV12;
							frame->linesize[0]=frame->width;
							frame->linesize[1]=frame->width;
							frame->pkt_pts = frame->pts = outsurf->Data.TimeStamp;

							frame->repeat_pict =outsurf->Info.PicStruct & MFX_PICSTRUCT_FRAME_TRIPLING ? 4 :
								outsurf->Info.PicStruct & MFX_PICSTRUCT_FRAME_DOUBLING ? 2 :
								outsurf->Info.PicStruct & MFX_PICSTRUCT_FIELD_REPEATED ? 1 : 0;
							frame->top_field_first =
								outsurf->Info.PicStruct & MFX_PICSTRUCT_FIELD_TFF;
							frame->interlaced_frame =
								!(outsurf->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);
							*got_frame = 1;
							//解锁
							qsv_unlock_used_frames(q);
				 }	
        }
  
   
    if (MFX_ERR_MORE_DATA!=ret && ret < 0) {
      
        av_log(avctx, AV_LOG_ERROR, "Error %d during QSV decoding.\n", ret);
        return ff_qsv_error(ret);
    }

    return avpkt->size;
}
/*
 This function inserts a packet at fifo front.
*/
static void qsv_packet_push_front(KKQSVContext *q, AVPacket *avpkt)
{
    int fifo_size = av_fifo_size(q->pkt_fifo);
    if (!fifo_size) {
    /* easy case fifo is empty */
        av_fifo_generic_write(q->pkt_fifo, avpkt, sizeof(*avpkt), NULL);
    } else {
    /* realloc necessary */
        AVPacket pkt;
        AVFifoBuffer *fifo = av_fifo_alloc(fifo_size+av_fifo_space(q->pkt_fifo));

        av_fifo_generic_write(fifo, avpkt, sizeof(*avpkt), NULL);

        while (av_fifo_size(q->pkt_fifo)) {
            av_fifo_generic_read(q->pkt_fifo, &pkt, sizeof(pkt), NULL);
            av_fifo_generic_write(fifo,       &pkt, sizeof(pkt), NULL);
        }
        av_fifo_free(q->pkt_fifo);
        q->pkt_fifo = fifo;
    }
}
int ff_qsv_decode(AVCodecContext *avctx, KKQSVContext *q,
                  AVFrame *frame, int *got_frame,
                  AVPacket *avpkt)
{
    AVPacket pkt_ref = { 0 };
    int ret = 0;

    if (q->pkt_fifo && av_fifo_size(q->pkt_fifo) >= sizeof(AVPacket)) {
        /* we already have got some buffered packets. so add new to tail */
        ret = av_packet_ref(&pkt_ref, avpkt);
        if (ret < 0)
            return ret;
        av_fifo_generic_write(q->pkt_fifo, &pkt_ref, sizeof(pkt_ref), NULL);
    }
    if (q->reinit_pending) {
        ret = do_qsv_decode(avctx, q, frame, got_frame, avpkt);

        if (!*got_frame) {
            /* Flushing complete, no more frames  */
            close_decoder(q);
            //return ff_qsv_decode(avctx, q, frame, got_frame, avpkt);
        }
    }
    if (!q->reinit_pending) {
        if (q->pkt_fifo && av_fifo_size(q->pkt_fifo) >= sizeof(AVPacket)) {
            /* process buffered packets */
            while (!*got_frame && av_fifo_size(q->pkt_fifo) >= sizeof(AVPacket)) {
                av_fifo_generic_read(q->pkt_fifo, &pkt_ref, sizeof(pkt_ref), NULL);
                ret = do_qsv_decode(avctx, q, frame, got_frame, &pkt_ref);
                if (q->reinit_pending) {
                    /*
                       A rare case: new reinit pending when buffering existing.
                       We should to return the pkt_ref back to same place of fifo
                    */
                    qsv_packet_push_front(q, &pkt_ref);
                } else {
                    av_packet_unref(&pkt_ref);
                }
           }
        } else {
            /* general decoding */
            ret = do_qsv_decode(avctx, q, frame, got_frame, avpkt);
            if (q->reinit_pending) {
                ret = av_packet_ref(&pkt_ref, avpkt);
                if (ret < 0)
                    return ret;
                av_fifo_generic_write(q->pkt_fifo, &pkt_ref, sizeof(pkt_ref), NULL);
            }
        }
    }

    return ret;
}

void RestQsvSurface(AVCodecContext *avctx,int idx);
/*
 This function resets decoder and corresponded buffers before seek operation
*/
void kk_qsv_decode_flush(AVCodecContext *avctx, KKQSVContext *q)
{



	
    KKQSVFrame *cur;
    AVPacket pkt;
    int ret = 0;
    mfxVideoParam param = { { 0 } };

//    if (q->reinit_pending) {
//        close_decoder(q);
//    } else if (q->engine_ready) {
//        ret = MFXVideoDECODE_GetVideoParam(q->session, &param);
//        if (ret < 0) {
//            av_log(avctx, AV_LOG_ERROR, "MFX decode get param error %d\n", ret);
//        }
//
//       /* ret = MFXVideoDECODE_Reset(q->session, &param);
//        if (ret < 0) {
//            av_log(avctx, AV_LOG_ERROR, "MFX decode reset error %d\n", ret);
//        }*/
////解锁
//		//qsv_unlock_used_frames(q);
//        /* Free all frames*/
//       /* cur = q->work_frames;
//		int xx=0;
//        while (cur) {
//            q->work_frames = cur->next;
//			av_frame_unref(cur->frame);
//            av_frame_free(&cur->frame);
//            av_freep(&cur);
//            cur = q->work_frames;
//			xx++;
//        }
//		xx++;*/
//		//RestQsvSurface(avctx,xx);
//    }
	/*if (q->engine_ready){
        qsv_unlock_used_frames(q);
	    cur = q->work_frames;
		int xx=0;
        while (cur) {
			q->work_frames = cur->next;
			av_frame_unref(cur->frame);
            av_frame_free(&cur->frame);
            av_freep(&cur);
            cur = q->work_frames;
        }
		if(xx<4)
			xx=4;
        RestQsvSurface(avctx,xx);
	}*/
     q->orig_pix_fmt = AV_PIX_FMT_NONE;
    /* Reset input packets fifo */
	 if(q->pkt_fifo)
	 {
         while (av_fifo_size(q->pkt_fifo)) {
             av_fifo_generic_read(q->pkt_fifo, &pkt, sizeof(pkt), NULL);
             av_packet_unref(&pkt);
        }
	 }
	 /*if(q->input_fifo)
	 {
         av_fifo_reset(q->input_fifo);
	 }*/
    /* Reset input bitstream fifo
	if(q->input_fifo)
	{
      av_fifo_reset(q->input_fifo);
	  if(q->engine_ready&&h265H264DecodeHeader)
	  {

		  int sziex= av_fifo_size(q->sequence_fifo);
	      av_fifo_generic_write(q->input_fifo, q->sequence_fifo->rptr, sziex, NULL);
	  }
	} */

	
}

int ff_qsv_decode_close(KKQSVContext *q)
{
    close_decoder(q);

    q->session = NULL;

    ff_qsv_close_internal_session(&q->internal_qs);

  
	if(q->input_fifo){
		av_fifo_free(q->input_fifo);
		q->input_fifo = NULL;
	}
   
	if(q->pkt_fifo){
       av_fifo_free(q->pkt_fifo);
       q->pkt_fifo = NULL;
	}

	if(q->sequence_fifo){
	   av_fifo_free(q->sequence_fifo);
       q->sequence_fifo = NULL;
	}
	
    return 0;
}

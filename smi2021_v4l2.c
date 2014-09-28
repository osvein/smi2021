/************************************************************************
 * smi2021_v4l2.c							*
 *									*
 * USB Driver for smi2021 - EasyCap					*
 * **********************************************************************
 *
 * Copyright 2011-2013 Jon Arne Jørgensen
 * <jonjon.arnearne--a.t--gmail.com>
 *
 * Copyright 2011, 2012 Tony Brown, Michal Demin, Jeffry Johnston
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * This driver is heavily influensed by the STK1160 driver.
 * Copyright (C) 2012 Ezequiel Garcia
 * <elezegarcia--a.t--gmail.com>
 *
 */

#include "smi2021.h"

static int vidioc_querycap(struct file *file, void *priv,
			struct v4l2_capability *cap)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	strlcpy(cap->driver, "smi2021", sizeof(cap->driver));
	strlcpy(cap->card, "smi2021", sizeof(cap->card));
	usb_make_path(smi2021->udev, cap->bus_info, sizeof(cap->bus_info));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE |
			   V4L2_CAP_STREAMING |
			   V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *i)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	if (i->index >= smi2021->vid_input_count)
		return -EINVAL;

	strlcpy(i->name, smi2021->vid_inputs[i->index].name, sizeof(i->name));
	i->type = V4L2_INPUT_TYPE_CAMERA;
	i->std = smi2021->vdev.tvnorms;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_fmtdesc *f)
{
	if (f->index != 0)
		return -EINVAL;

	strlcpy(f->description, "16bpp YU2, 4:2:2, packed",
					sizeof(f->description));
	f->pixelformat = V4L2_PIX_FMT_UYVY;
	return 0;
}

static int vidioc_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	f->fmt.pix.width = SMI2021_BYTES_PER_LINE / 2;
	f->fmt.pix.height = smi2021->cur_height;
	f->fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	f->fmt.pix.bytesperline = SMI2021_BYTES_PER_LINE;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	f->fmt.pix.priv = 0;
	return 0;
}

static int vidioc_g_std(struct file *file, void *priv, v4l2_std_id *norm)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	*norm = smi2021->cur_norm;
	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	*i = smi2021->cur_input;
	return 0;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id norm)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	if (norm == smi2021->cur_norm)
		return 0;

	if (vb2_is_busy(&smi2021->vb2q))
		return -EBUSY;

	smi2021->cur_norm = norm;
	if (norm & V4L2_STD_525_60)
		smi2021->cur_height = SMI2021_NTSC_LINES;
	else if (norm & V4L2_STD_625_50)
		smi2021->cur_height = SMI2021_PAL_LINES;
	else
		return -EINVAL;

	v4l2_subdev_call(smi2021->gm7113c_subdev, video, s_std,
			smi2021->cur_norm);

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct smi2021 *smi2021 = video_drvdata(file);

	if (i >= smi2021->vid_input_count)
		return -EINVAL;

	v4l2_subdev_call(smi2021->gm7113c_subdev, video, s_routing,
		smi2021->vid_inputs[i].type, 0, 0);

	smi2021->cur_input = i;

	return 0;
}

static const struct v4l2_ioctl_ops smi2021_ioctl_ops = {
	.vidioc_querycap		= vidioc_querycap,
	.vidioc_enum_input		= vidioc_enum_input,
	.vidioc_enum_fmt_vid_cap	= vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= vidioc_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= vidioc_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= vidioc_fmt_vid_cap,
	.vidioc_g_std			= vidioc_g_std,
	.vidioc_s_std			= vidioc_s_std,
	.vidioc_g_input			= vidioc_g_input,
	.vidioc_s_input			= vidioc_s_input,

	/* vb2 handle these */
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,

	/* v4l2-event and v4l2-cntrl handle these */
	.vidioc_log_status		= v4l2_ctrl_log_status,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static struct v4l2_file_operations smi2021_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

/*
 * Videobuf2 operations
 */
static int queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *v4l2_fmt,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], void *alloc_ctxs[])
{
	struct smi2021 *smi2021 = vb2_get_drv_priv(vq);
	*nbuffers = clamp_t(unsigned int, *nbuffers, 4, 16);

	*nplanes = 1;
	sizes[0] = SMI2021_BYTES_PER_LINE * smi2021->cur_height;

	return 0;
}

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct smi2021 *smi2021 = vb2_get_drv_priv(vb->vb2_queue);
	struct smi2021_buf *buf = container_of(vb, struct smi2021_buf, vb);
	unsigned long size;

	size = smi2021->cur_height * SMI2021_BYTES_PER_LINE;

	if (vb2_plane_size(vb, 0) < size) {
		smi2021_dbg("%s data will not fit into plane (%lu < %lu)\n",
				__func__, vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}
	vb2_set_plane_payload(&buf->vb, 0, size);

	buf->pos = 0;
	buf->trc_av = 0;
	buf->in_blank = true;
	buf->second_field = false;

	return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	unsigned long flags;
	struct smi2021 *smi2021 = vb2_get_drv_priv(vb->vb2_queue);
	struct smi2021_buf *buf = container_of(vb, struct smi2021_buf, vb);

	buf->mem = vb2_plane_vaddr(vb, 0);
	buf->length = vb2_plane_size(vb, 0);

	spin_lock_irqsave(&smi2021->buf_lock, flags);
	list_add_tail(&buf->list, &smi2021->bufs);
	spin_unlock_irqrestore(&smi2021->buf_lock, flags);
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct smi2021 *smi2021 = vb2_get_drv_priv(vq);
	unsigned long flags;
	int rc;

	if (smi2021->udev == NULL)
		goto error;

	rc = smi2021_start(smi2021);
	if (rc)
		goto error;


	return 0;

error:
	spin_lock_irqsave(&smi2021->buf_lock, flags);
	while (!list_empty(&smi2021->bufs)) {
		struct smi2021_buf *buf = list_first_entry(&smi2021->bufs,
						struct smi2021_buf, list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&smi2021->buf_lock, flags);
	return -ENODEV;

}

static void stop_streaming(struct vb2_queue *vq)
{
	struct smi2021 *smi2021 = vb2_get_drv_priv(vq);
	unsigned long flags;

	smi2021_stop(smi2021);

	/* Return buffers to userspace */
	spin_lock_irqsave(&smi2021->buf_lock, flags);
	while (!list_empty(&smi2021->bufs)) {
		struct smi2021_buf *buf = list_first_entry(&smi2021->bufs,
						struct smi2021_buf, list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&smi2021->buf_lock, flags);

	/* And release the current active buffer (if any) */
	spin_lock(&smi2021->slock);
	if (smi2021->cur_buf) {
		vb2_buffer_done(&smi2021->cur_buf->vb, VB2_BUF_STATE_ERROR);
		smi2021->cur_buf = NULL;
	}
	spin_unlock(&smi2021->slock);

	return;
}

static struct vb2_ops smi2021_vb2_ops = {
	.queue_setup		= queue_setup,
	.buf_prepare		= buffer_prepare,
	.buf_queue		= buffer_queue,
	.start_streaming	= start_streaming,
	.stop_streaming		= stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

int smi2021_vb2_setup(struct smi2021 *smi2021)
{
	smi2021->vb2q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	smi2021->vb2q.io_modes = VB2_READ | VB2_MMAP | VB2_USERPTR;
	smi2021->vb2q.drv_priv = smi2021;
	smi2021->vb2q.buf_struct_size = sizeof(struct smi2021_buf);
	smi2021->vb2q.ops = &smi2021_vb2_ops;
	smi2021->vb2q.mem_ops = &vb2_vmalloc_memops;
	smi2021->vb2q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	smi2021->vb2q.lock = &smi2021->vb2q_lock;

	return vb2_queue_init(&smi2021->vb2q);
}

int smi2021_video_register(struct smi2021 *smi2021)
{
	strlcpy(smi2021->vdev.name, "smi2021", sizeof(smi2021->vdev.name));
	smi2021->vdev.v4l2_dev = &smi2021->v4l2_dev;
	smi2021->vdev.release = video_device_release_empty;
	smi2021->vdev.fops = &smi2021_fops;
	smi2021->vdev.ioctl_ops = &smi2021_ioctl_ops;
	smi2021->vdev.tvnorms = V4L2_STD_ALL;
	smi2021->vdev.queue = &smi2021->vb2q;
	smi2021->vdev.lock = &smi2021->v4l2_lock;
	set_bit(V4L2_FL_USE_FH_PRIO, &smi2021->vdev.flags);
	video_set_drvdata(&smi2021->vdev, smi2021);

	return video_register_device(&smi2021->vdev, VFL_TYPE_GRABBER, -1);
}

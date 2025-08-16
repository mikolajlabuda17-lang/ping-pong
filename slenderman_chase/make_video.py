#!/usr/bin/env python3
import os
import sys
import math
import tarfile
import shutil
import stat
import subprocess
from urllib.request import urlretrieve

# Configuration
WIDTH = 640  # Lower resolution for speed; can be raised to 1280
HEIGHT = 360
FPS = 24
DURATION = 8.0
TOTAL_FRAMES = int(DURATION * FPS)
OUTPUT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "."))
FRAMES_DIR = os.path.join(OUTPUT_DIR, "frames")
OUTPUT_MP4 = os.path.join(OUTPUT_DIR, "slenderman_chase.mp4")
FFMPEG_DIR = os.path.join(OUTPUT_DIR, "ffmpeg-static")
FFMPEG_BIN = os.path.join(FFMPEG_DIR, "ffmpeg")

# Colors (RGB 0-255)
BG_COLOR = (8, 8, 10)  # Night sky
GROUND_COLOR = (18, 25, 18)
HUMAN_COLOR = (230, 230, 230)  # light gray silhouette
SLENDER_COLOR = (10, 10, 10)  # near-black
TENTACLE_COLOR = (18, 18, 18)


def ensure_dirs():
	os.makedirs(FRAMES_DIR, exist_ok=True)
	os.makedirs(FFMPEG_DIR, exist_ok=True)


def download_ffmpeg_if_needed() -> str:
	"""Download a static ffmpeg binary if not present, return path to ffmpeg."""
	if os.path.isfile(FFMPEG_BIN) and os.access(FFMPEG_BIN, os.X_OK):
		return FFMPEG_BIN

	# Try John Van Sickle static release
	url = "https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz"
	archive_path = os.path.join(FFMPEG_DIR, "ffmpeg-release-amd64-static.tar.xz")
	print(f"Downloading ffmpeg static build...\n{url}")
	urlretrieve(url, archive_path)
	print("Extracting ffmpeg archive...")
	with tarfile.open(archive_path, mode="r:xz") as tf:
		# Extract only the ffmpeg binary
		member_ffmpeg = None
		for m in tf.getmembers():
			# Path typically like: ffmpeg-<ver>-amd64-static/ffmpeg
			base = os.path.basename(m.name)
			if base == "ffmpeg":
				member_ffmpeg = m
				break
		if member_ffmpeg is None:
			raise RuntimeError("ffmpeg binary not found inside archive")
		member_ffmpeg.name = os.path.basename(member_ffmpeg.name)  # strip directories
		tf.extract(member_ffmpeg, path=FFMPEG_DIR)

	# Rename to target path
	extracted_path = os.path.join(FFMPEG_DIR, "ffmpeg")
	if not os.path.exists(extracted_path):
		# Some archives may extract to another name; search just in case
		for root, _, files in os.walk(FFMPEG_DIR):
			if "ffmpeg" in files:
				extracted_path = os.path.join(root, "ffmpeg")
				break
	shutil.move(extracted_path, FFMPEG_BIN) if extracted_path != FFMPEG_BIN else None
	os.chmod(FFMPEG_BIN, os.stat(FFMPEG_BIN).st_mode | stat.S_IEXEC)
	print(f"ffmpeg ready at: {FFMPEG_BIN}")
	return FFMPEG_BIN


# --- Minimal software rasterizer helpers ---

def make_frame_buffer(color=(0, 0, 0)):
	return bytearray(bytes([color[0], color[1], color[2]]) * (WIDTH * HEIGHT))


def clamp(v, lo, hi):
	return lo if v < lo else hi if v > hi else v


def put_pixel(buf: bytearray, x: int, y: int, color):
	if 0 <= x < WIDTH and 0 <= y < HEIGHT:
		idx = (y * WIDTH + x) * 3
		buf[idx] = color[0]
		buf[idx + 1] = color[1]
		buf[idx + 2] = color[2]


def draw_filled_rect(buf: bytearray, x: int, y: int, w: int, h: int, color):
	x0 = clamp(x, 0, WIDTH)
	y0 = clamp(y, 0, HEIGHT)
	x1 = clamp(x + w, 0, WIDTH)
	y1 = clamp(y + h, 0, HEIGHT)
	if x0 >= x1 or y0 >= y1:
		return
	for yy in range(y0, y1):
		row_idx = (yy * WIDTH + x0) * 3
		span = (x1 - x0) * 3
		buf[row_idx:row_idx + span] = bytes([color[0], color[1], color[2]]) * (x1 - x0)


def draw_filled_circle(buf: bytearray, cx: int, cy: int, r: int, color):
	r2 = r * r
	y_start = clamp(cy - r, 0, HEIGHT)
	y_end = clamp(cy + r + 1, 0, HEIGHT)
	for y in range(y_start, y_end):
		dy = y - cy
		dx_max_sq = r2 - dy * dy
		if dx_max_sq < 0:
			continue
		dx = int(math.sqrt(dx_max_sq))
		x_start = clamp(cx - dx, 0, WIDTH)
		x_end = clamp(cx + dx + 1, 0, WIDTH)
		row_idx = (y * WIDTH + x_start) * 3
		span_px = (x_end - x_start)
		if span_px <= 0:
			continue
		buf[row_idx:row_idx + span_px * 3] = bytes([color[0], color[1], color[2]]) * span_px


def draw_line(buf: bytearray, x0: int, y0: int, x1: int, y1: int, color):
	dx = abs(x1 - x0)
	dy = -abs(y1 - y0)
	sx = 1 if x0 < x1 else -1
	sy = 1 if y0 < y1 else -1
	err = dx + dy
	while True:
		put_pixel(buf, x0, y0, color)
		if x0 == x1 and y0 == y1:
			break
		e2 = 2 * err
		if e2 >= dy:
			err += dy
			x0 += sx
		if e2 <= dx:
			err += dx
			y0 += sy


def draw_thick_line(buf: bytearray, x0: int, y0: int, x1: int, y1: int, thickness: int, color):
	# Draw a polyline of small disks along a standard line for thickness
	# Step along the longer axis
	dx = x1 - x0
	dy = y1 - y0
	length = max(abs(dx), abs(dy))
	if length == 0:
		draw_filled_circle(buf, x0, y0, max(1, thickness // 2), color)
		return
	for i in range(length + 1):
		t = i / length
		x = int(round(x0 + dx * t))
		y = int(round(y0 + dy * t))
		draw_filled_circle(buf, x, y, max(1, thickness // 2), color)


# --- Scene drawing ---

def draw_scene(buf: bytearray, t: float):
	# Background
	buf[:] = bytes([BG_COLOR[0], BG_COLOR[1], BG_COLOR[2]]) * (WIDTH * HEIGHT)

	# Ground
	ground_y = int(HEIGHT * 0.72)
	draw_filled_rect(buf, 0, ground_y, WIDTH, HEIGHT - ground_y, GROUND_COLOR)

	# Positions over time
	# Human moves from left to right
	start_x = int(WIDTH * 0.10)
	end_x = int(WIDTH * 0.78)
	human_x = int(start_x + (end_x - start_x) * (t / DURATION))
	human_y = ground_y - 48

	# Slenderman chases from behind, slightly faster so he closes the gap
	start_gap = int(WIDTH * 0.28)
	close_rate = 0.55  # 0..1 fraction of gap closed over DURATION
	slend_x = int(human_x - (start_gap * (1.0 - close_rate * (t / DURATION))))
	slend_y = ground_y - 110

	# Animate gait
	step_period = 0.8  # seconds per step
	phase = (t % step_period) / step_period  # 0..1
	angle = math.sin(phase * 2 * math.pi)

	# Draw human (simple stick figure + torso)
	head_r = 9
	draw_filled_circle(buf, human_x, human_y - 24, head_r, HUMAN_COLOR)
	# Torso
	draw_filled_rect(buf, human_x - 6, human_y - 22, 12, 30, HUMAN_COLOR)
	# Arms swinging
	arm_len = 24
	arm_thk = 4
	arm_angle = angle * 30 * math.pi / 180
	ax1 = human_x
	ay1 = human_y - 12
	ax2 = int(ax1 + arm_len * math.sin(arm_angle))
	ay2 = int(ay1 + arm_len * math.cos(arm_angle))
	draw_thick_line(buf, ax1, ay1, ax2, ay2, arm_thk, HUMAN_COLOR)
	# Opposite arm
	ax2b = int(ax1 - arm_len * math.sin(arm_angle))
	ay2b = int(ay1 + arm_len * math.cos(arm_angle))
	draw_thick_line(buf, ax1, ay1, ax2b, ay2b, arm_thk, HUMAN_COLOR)
	# Legs
	leg_len = 30
	leg_thk = 5
	leg_angle = angle * 25 * math.pi / 180
	lx1 = human_x - 4
	ly1 = human_y + 8
	lx2 = int(lx1 + leg_len * math.sin(leg_angle))
	ly2 = int(ly1 + leg_len * math.cos(leg_angle))
	draw_thick_line(buf, lx1, ly1, lx2, ly2, leg_thk, HUMAN_COLOR)
	# Opposite leg
	lx1b = human_x + 4
	ly1b = human_y + 8
	lx2b = int(lx1b - leg_len * math.sin(leg_angle))
	ly2b = int(ly1b + leg_len * math.cos(leg_angle))
	draw_thick_line(buf, lx1b, ly1b, lx2b, ly2b, leg_thk, HUMAN_COLOR)

	# Draw Slenderman (tall slender figure with tentacles)
	s_head_r = 10
	draw_filled_circle(buf, slend_x, slend_y - 38, s_head_r, (230, 230, 230))  # pale head
	# Torso (suit)
	draw_filled_rect(buf, slend_x - 7, slend_y - 36, 14, 60, SLENDER_COLOR)
	# Long legs
	s_leg_len = 48
	s_leg_thk = 6
	draw_thick_line(buf, slend_x - 4, slend_y + 22, slend_x - 4, slend_y + 22 + s_leg_len, s_leg_thk, SLENDER_COLOR)
	draw_thick_line(buf, slend_x + 4, slend_y + 22, slend_x + 4, slend_y + 22 + s_leg_len, s_leg_thk, SLENDER_COLOR)
	# Tentacles trailing back (animated slight sway)
	sway = math.sin((t * 0.8) * 2 * math.pi) * 18
	for i in range(4):
		offset = i * 10
		x1 = slend_x
		y1 = slend_y - 20 + i * 4
		x2 = int(slend_x - 50 - offset + sway * (0.5 + 0.1 * i))
		y2 = int(slend_y - 40 - i * 6)
		draw_thick_line(buf, x1, y1, x2, y2, 3, TENTACLE_COLOR)


def write_ppm(path: str, buf: bytearray):
	head = f"P6\n{WIDTH} {HEIGHT}\n255\n".encode("ascii")
	with open(path, "wb") as f:
		f.write(head)
		f.write(buf)


def generate_frames():
	print(f"Generating {TOTAL_FRAMES} frames at {WIDTH}x{HEIGHT}...")
	for i in range(TOTAL_FRAMES):
		t = i / FPS
		buf = make_frame_buffer(BG_COLOR)
		draw_scene(buf, t)
		frame_path = os.path.join(FRAMES_DIR, f"frame{i:04d}.ppm")
		write_ppm(frame_path, buf)
		if (i + 1) % 24 == 0 or i == TOTAL_FRAMES - 1:
			print(f"  {i + 1}/{TOTAL_FRAMES} frames")


def encode_video(ffmpeg_path: str):
	print("Encoding MP4 with ffmpeg...")
	cmds = [
		[ffmpeg_path, "-y", "-framerate", str(FPS), "-i", os.path.join(FRAMES_DIR, "frame%04d.ppm"), "-vf", "format=yuv420p", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-movflags", "+faststart", OUTPUT_MP4],
		[ffmpeg_path, "-y", "-framerate", str(FPS), "-i", os.path.join(FRAMES_DIR, "frame%04d.ppm"), "-vf", "format=yuv420p", "-c:v", "mpeg4", OUTPUT_MP4],
		[ffmpeg_path, "-y", "-framerate", str(FPS), "-i", os.path.join(FRAMES_DIR, "frame%04d.ppm"), "-vf", "format=yuv420p", "-c:v", "libx265", OUTPUT_MP4],
	]
	last_err = None
	for cmd in cmds:
		try:
			print("Running:", " ".join(cmd))
			subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			print(f"Wrote {OUTPUT_MP4}")
			return
		except subprocess.CalledProcessError as e:
			last_err = e
			print("ffmpeg attempt failed, trying fallback...")
	if last_err:
		raise RuntimeError(f"ffmpeg encoding failed: {last_err.stderr.decode(errors='ignore')}")


def main():
	ensure_dirs()
	ffmpeg_path = download_ffmpeg_if_needed()
	generate_frames()
	encode_video(ffmpeg_path)
	print("Done.")


if __name__ == "__main__":
	try:
		main()
	except Exception as exc:
		print("ERROR:", exc)
		sys.exit(1)
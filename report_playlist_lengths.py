#!/usr/bin/env python3

import argparse
import json
import shutil
import subprocess
import sys
import wave
from pathlib import Path


try:
    import soundfile  # type: ignore
except ImportError:
    soundfile = None


def read_playlist(playlist_path: Path) -> list[Path]:
    with playlist_path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)

    tracks = payload.get("tracks")
    if not isinstance(tracks, list):
        raise ValueError("Invalid VidMod playlist: missing 'tracks' array")

    resolved_tracks: list[Path] = []
    for index, track in enumerate(tracks, start=1):
        if not isinstance(track, dict) or not isinstance(track.get("filePath"), str):
            raise ValueError(f"Invalid VidMod playlist: track {index} missing string filePath")
        resolved_tracks.append(Path(track["filePath"]))

    return resolved_tracks


def duration_with_soundfile(audio_path: Path) -> float | None:
    if soundfile is None:
        return None

    try:
        info = soundfile.info(str(audio_path))
        return float(info.frames) / float(info.samplerate)
    except Exception:
        return None


def duration_with_ffprobe(audio_path: Path) -> float | None:
    ffprobe = shutil.which("ffprobe")
    if ffprobe is None:
        return None

    command = [
        ffprobe,
        "-v",
        "error",
        "-show_entries",
        "format=duration",
        "-of",
        "default=noprint_wrappers=1:nokey=1",
        str(audio_path),
    ]

    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        return float(result.stdout.strip())
    except Exception:
        return None


def duration_with_wave(audio_path: Path) -> float | None:
    if audio_path.suffix.lower() not in {".wav", ".wave"}:
        return None

    try:
        with wave.open(str(audio_path), "rb") as handle:
            frame_rate = handle.getframerate()
            frame_count = handle.getnframes()
            if frame_rate <= 0:
                return None
            return float(frame_count) / float(frame_rate)
    except Exception:
        return None


def get_duration_seconds(audio_path: Path) -> float:
    for probe in (duration_with_soundfile, duration_with_ffprobe, duration_with_wave):
        duration = probe(audio_path)
        if duration is not None:
            return max(0.0, duration)

    raise RuntimeError(
        "Could not determine duration. Install python-soundfile or ensure ffprobe is available."
    )


def format_mmss(duration_seconds: float) -> str:
    total_seconds = max(0, int(round(duration_seconds)))
    minutes, seconds = divmod(total_seconds, 60)
    return f"{minutes:02d}:{seconds:02d}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Report track names and lengths from a VidMod playlist JSON file."
    )
    parser.add_argument("playlist", type=Path, help="Path to a VidMod playlist JSON file")
    args = parser.parse_args()

    playlist_path = args.playlist.expanduser().resolve()
    if not playlist_path.is_file():
        print(f"Playlist not found: {playlist_path}", file=sys.stderr)
        return 1

    try:
        tracks = read_playlist(playlist_path)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if not tracks:
        print("Playlist is empty")
        return 0

    total_duration = 0.0
    for track_path in tracks:
        try:
            duration = get_duration_seconds(track_path)
            total_duration += duration
            print(f"{track_path.name}: {format_mmss(duration)}")
        except Exception as exc:
            print(f"{track_path.name}: error ({exc})")

    print(f"Total: {format_mmss(total_duration)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
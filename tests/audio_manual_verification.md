# Audio Module Manual Verification

This checklist covers the refactored audio UI and service boundaries. Run it after building `QtMediaPlayer` from a clean qmake build.

## PlaylistModel

- Add three local audio URLs and confirm `count()` becomes 3 while `currentIndex()` stays `-1` until explicitly set.
- Set current index to `0`, call `moveToNext()` in list-loop mode, and confirm indices advance `0 -> 1 -> 2 -> 0`.
- Switch to single-loop mode and confirm end-of-media handling keeps the same current index.
- Switch to random mode and call `moveToNext()` several times; confirm the index always stays within `[0, count - 1]`.
- Remove an item before the current song and confirm `currentIndex()` shifts down by one.
- Remove the current last item and confirm the next current index wraps to `0`.
- Clear the playlist and confirm `count() == 0`, `currentIndex() == -1`, and `hasCurrent() == false`.

## AudioPlaybackController

- Construct the controller and confirm `player()` and `audioOutput()` are non-null.
- Open a known local audio URL and confirm the media source changes.
- Set volume to `80`, `0`, and `120`; confirm returned volume is clamped to `80`, `0`, and `100`.
- Call `play()`, `pause()`, and `stop()` with a valid source and confirm the `QMediaPlayer` playback state follows.
- Temporarily clear the player's audio output, call `ensureAudioOutput()`, and confirm the controller output is reattached.

## LyricService

- Use an audio file with a matching local `.lrc` file and confirm `lyricsReady` is emitted.
- Use an audio file without local lyrics and confirm `lyricsCleared` and a download status message are emitted.
- Start loading lyrics for track A, then immediately track B; if A's async download returns later, confirm it is ignored.
- Pass an empty audio path and confirm lyrics are cleared without a download request.

## Audio Regression

- Local playback: add a local song, play, pause, resume, seek, and change volume.
- Track switching: use previous and next buttons across the first and last items.
- Play modes: verify list loop, single loop, and random mode from the control bar.
- Delete song: delete a non-current song, then delete the currently playing song.
- Clear list: clear a populated playlist and confirm playback stops, lyrics clear, and spectrum stops.
- Online search: search a keyword and confirm results show as candidate songs; disable network and confirm a clear search failure is shown.
- Online playback: add a search result, confirm it appears in the playlist, then start playback and confirm the real URL is resolved before audio loads.
- Online playback failure: choose a candidate whose URL cannot be resolved or disable network before playback, and confirm the playlist item shows a failed state with "播放地址不可用".
- Lyrics: play a local file with lyrics, then one without lyrics, and confirm the lyric panel updates or clears.
- Login: open the login menu from the playlist panel and confirm user label updates after login/logout.
- Password change: open the user menu and verify the change-password flow still opens and validates input.

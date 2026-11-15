// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs (tools/shader_plan.json) and play_page.mjs
// (tools/play_plan.json): reads where the game is from RDRAM, so a plan presses on through the menus by what the game
// shows rather than by fixed times. The addresses are the USA ROM's (syms/). It offers
//
//   clock()          the game's frames (0x800B67F0, which the graphics callback counts up a game frame, 30 a second as
//                    the game runs), for a plan's ["clock", "clock()", 30]
//   loaded(i)        whether overlay i is in memory: the loaded byte at the head of its entry in the overlay table
//                    (0x800B5F58, 0x28 bytes an overlay), which the game's loader sets. The screens a plan meets, by the
//                    overlays they load: 4 and 7 the intro (its golfers one by one), 4 and 8 the title, 3 and 4 the main
//                    menu and the menus after it, 2, 4 and 11 a course loading and its hole's flyover, 2 and 11 a hole
//                    played, 6 beside them as a shot flies
//   overlays()       the loaded overlays' numbers, "2,11", for a plan's log
//   until(test, ms)  resolves true once test() is, polled every 100 ms, or false after ms
(() => {
  const r8 = (a) => (n64r(a & ~3) >>> (24 - 8 * (a & 3))) & 0xFF;
  const OVERLAYS = 0x800B5F58, OVERLAY_SIZE = 0x28, OVERLAY_COUNT = 17, FRAMES = 0x800B67F0;
  window.clock = () => n64r(FRAMES);
  window.loaded = (i) => r8(OVERLAYS + i * OVERLAY_SIZE) !== 0;
  window.overlays = () => {
    const list = [];
    for (let i = 0; i < OVERLAY_COUNT; i++) if (loaded(i)) list.push(i);
    return list.join(',');
  };
  window.until = (test, ms) => new Promise((resolve) => {
    const from = Date.now();
    const poll = () => {
      if (test()) resolve(true);
      else if (Date.now() - from > ms) resolve(false);
      else setTimeout(poll, 100);
    };
    poll();
  });
  return 'survey: ' + overlays();
})()

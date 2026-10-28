/* Shared logical display contract for every game made for this console.
   Keep independent from viewport pixels: render at 208x320, then scale to the fixed display aperture. */
(function(){
  'use strict';
  const screen = Object.freeze({ width: 208, height: 320, aspect: 13 / 20, orientation: 'portrait' });
  Object.defineProperty(window, 'CONSOLE_GAME_SCREEN', {
    value: screen,
    enumerable: true,
    configurable: false,
    writable: false
  });
})();
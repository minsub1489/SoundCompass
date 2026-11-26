// Time beyond maxSteps is dropped so a long stall (tab switch) cannot snowball.
export function stepCount(accumulator, frameDt, fixedDt, maxSteps) {
  let acc = accumulator + frameDt;
  let steps = Math.floor(acc / fixedDt + 1e-9);
  if (steps > maxSteps) return { steps: maxSteps, accumulator: 0 };
  acc -= steps * fixedDt;
  return { steps, accumulator: Math.max(0, acc) };
}

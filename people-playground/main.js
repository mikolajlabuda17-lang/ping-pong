/* global Matter */

(function () {
  const {
    Engine,
    Render,
    Runner,
    Composite,
    Composites,
    Constraint,
    Bodies,
    Body,
    Mouse,
    MouseConstraint,
    Vector,
    Events
  } = Matter;

  const container = document.getElementById('canvasContainer');

  const engine = Engine.create({ enableSleeping: true });
  const world = engine.world;
  world.gravity.y = 1;

  const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);

  const render = Render.create({
    element: container,
    engine,
    options: {
      wireframes: false,
      background: '#0b0d1a',
      pixelRatio,
    },
  });

  const runner = Runner.create();

  // Keep references to static boundaries to avoid clearing them
  const staticBoundaries = [];
  const ragdollBodies = new Set();

  function createBoundaries() {
    const width = container.clientWidth;
    const height = container.clientHeight;
    const thickness = 80;

    // Ground and walls
    const ground = Bodies.rectangle(width / 2, height + thickness / 2 - 2, width + thickness * 2, thickness, { isStatic: true, render: { fillStyle: '#22273f' } });
    const ceiling = Bodies.rectangle(width / 2, -thickness / 2, width + thickness * 2, thickness, { isStatic: true, render: { fillStyle: '#22273f' } });
    const leftWall = Bodies.rectangle(-thickness / 2, height / 2, thickness, height + thickness * 2, { isStatic: true, render: { fillStyle: '#22273f' } });
    const rightWall = Bodies.rectangle(width + thickness / 2, height / 2, thickness, height + thickness * 2, { isStatic: true, render: { fillStyle: '#22273f' } });

    staticBoundaries.push(ground, ceiling, leftWall, rightWall);
    Composite.add(world, staticBoundaries);
  }

  function clearAndRecreateBoundaries() {
    for (const b of staticBoundaries.splice(0)) {
      Composite.remove(world, b);
    }
    createBoundaries();
  }

  function markAsRagdollPart(body) {
    body.plugin = body.plugin || {};
    body.plugin.isRagdollPart = true;
    ragdollBodies.add(body);
  }

  function createRagdoll(centerX, centerY, scale = 1) {
    const headRadius = 14 * scale;
    const torsoWidth = 20 * scale;
    const torsoHeight = 40 * scale;
    const limbWidth = 8 * scale;
    const upperLimb = 18 * scale;
    const lowerLimb = 18 * scale;

    const head = Bodies.circle(centerX, centerY - torsoHeight - headRadius, headRadius, {
      restitution: 0.05,
      friction: 0.3,
      render: { fillStyle: '#d1d6ff' },
    });

    const upperTorso = Bodies.rectangle(centerX, centerY - torsoHeight * 0.6, torsoWidth, torsoHeight * 0.55, { render: { fillStyle: '#9aa5ff' } });
    const lowerTorso = Bodies.rectangle(centerX, centerY - torsoHeight * 0.05, torsoWidth, torsoHeight * 0.6, { render: { fillStyle: '#8b95f3' } });

    const leftUpperArm = Bodies.rectangle(centerX - torsoWidth * 0.8, centerY - torsoHeight * 0.6, limbWidth, upperLimb, { render: { fillStyle: '#c2c8ff' } });
    const leftLowerArm = Bodies.rectangle(centerX - torsoWidth * 0.8, centerY - torsoHeight * 0.35, limbWidth, lowerLimb, { render: { fillStyle: '#b3baff' } });

    const rightUpperArm = Bodies.rectangle(centerX + torsoWidth * 0.8, centerY - torsoHeight * 0.6, limbWidth, upperLimb, { render: { fillStyle: '#c2c8ff' } });
    const rightLowerArm = Bodies.rectangle(centerX + torsoWidth * 0.8, centerY - torsoHeight * 0.35, limbWidth, lowerLimb, { render: { fillStyle: '#b3baff' } });

    const leftUpperLeg = Bodies.rectangle(centerX - torsoWidth * 0.3, centerY + lowerLimb * 0.2, limbWidth, upperLimb + 6 * scale, { render: { fillStyle: '#aab2ff' } });
    const leftLowerLeg = Bodies.rectangle(centerX - torsoWidth * 0.3, centerY + upperLimb * 0.8, limbWidth, lowerLimb + 6 * scale, { render: { fillStyle: '#98a2ff' } });

    const rightUpperLeg = Bodies.rectangle(centerX + torsoWidth * 0.3, centerY + lowerLimb * 0.2, limbWidth, upperLimb + 6 * scale, { render: { fillStyle: '#aab2ff' } });
    const rightLowerLeg = Bodies.rectangle(centerX + torsoWidth * 0.3, centerY + upperLimb * 0.8, limbWidth, lowerLimb + 6 * scale, { render: { fillStyle: '#98a2ff' } });

    const stiff = 0.6;

    const neck = Constraint.create({ bodyA: head, pointA: { x: 0, y: headRadius * 0.6 }, bodyB: upperTorso, pointB: { x: 0, y: -torsoHeight * 0.25 }, stiffness: stiff, render: { strokeStyle: '#7aa2f7' } });

    const spine = Constraint.create({ bodyA: upperTorso, bodyB: lowerTorso, pointA: { x: 0, y: torsoHeight * 0.25 }, pointB: { x: 0, y: -torsoHeight * 0.2 }, stiffness: 0.9, render: { strokeStyle: '#7aa2f7' } });

    const lShoulder = Constraint.create({ bodyA: upperTorso, pointA: { x: -torsoWidth * 0.6, y: -torsoHeight * 0.15 }, bodyB: leftUpperArm, pointB: { x: 0, y: -upperLimb * 0.45 }, stiffness: stiff });
    const lElbow = Constraint.create({ bodyA: leftUpperArm, pointA: { x: 0, y: upperLimb * 0.45 }, bodyB: leftLowerArm, pointB: { x: 0, y: -lowerLimb * 0.45 }, stiffness: stiff });

    const rShoulder = Constraint.create({ bodyA: upperTorso, pointA: { x: torsoWidth * 0.6, y: -torsoHeight * 0.15 }, bodyB: rightUpperArm, pointB: { x: 0, y: -upperLimb * 0.45 }, stiffness: stiff });
    const rElbow = Constraint.create({ bodyA: rightUpperArm, pointA: { x: 0, y: upperLimb * 0.45 }, bodyB: rightLowerArm, pointB: { x: 0, y: -lowerLimb * 0.45 }, stiffness: stiff });

    const lHip = Constraint.create({ bodyA: lowerTorso, pointA: { x: -torsoWidth * 0.25, y: lowerLimb * 0.1 }, bodyB: leftUpperLeg, pointB: { x: 0, y: -upperLimb * 0.45 }, stiffness: stiff });
    const lKnee = Constraint.create({ bodyA: leftUpperLeg, pointA: { x: 0, y: upperLimb * 0.45 }, bodyB: leftLowerLeg, pointB: { x: 0, y: -lowerLimb * 0.45 }, stiffness: 0.55 });

    const rHip = Constraint.create({ bodyA: lowerTorso, pointA: { x: torsoWidth * 0.25, y: lowerLimb * 0.1 }, bodyB: rightUpperLeg, pointB: { x: 0, y: -upperLimb * 0.45 }, stiffness: stiff });
    const rKnee = Constraint.create({ bodyA: rightUpperLeg, pointA: { x: 0, y: upperLimb * 0.45 }, bodyB: rightLowerLeg, pointB: { x: 0, y: -lowerLimb * 0.45 }, stiffness: 0.55 });

    const parts = [
      head,
      upperTorso, lowerTorso,
      leftUpperArm, leftLowerArm,
      rightUpperArm, rightLowerArm,
      leftUpperLeg, leftLowerLeg,
      rightUpperLeg, rightLowerLeg,
    ];

    for (const p of parts) markAsRagdollPart(p);

    Composite.add(world, [
      ...parts,
      neck, spine,
      lShoulder, lElbow,
      rShoulder, rElbow,
      lHip, lKnee,
      rHip, rKnee,
    ]);

    // Small nudge so ragdoll is not entirely static
    for (const p of parts) {
      Body.setAngularVelocity(p, (Math.random() - 0.5) * 0.1);
    }

    return parts;
  }

  function addExplosion(pointX, pointY, power = 0.03) {
    const bodies = Composite.allBodies(world);
    const radius = 160;
    for (const body of bodies) {
      if (body.isStatic) continue;
      const distance = Vector.magnitude(Vector.sub(body.position, { x: pointX, y: pointY }));
      if (distance > radius) continue;
      const direction = Vector.normalise(Vector.sub(body.position, { x: pointX, y: pointY }));
      const falloff = 1 - distance / radius;
      const force = Vector.mult(direction, power * falloff * body.mass);
      Body.applyForce(body, body.position, force);
    }
  }

  function fitRenderToContainer() {
    const width = container.clientWidth;
    const height = container.clientHeight;

    render.options.width = width;
    render.options.height = height;

    render.canvas.width = Math.floor(width * pixelRatio);
    render.canvas.height = Math.floor(height * pixelRatio);
    render.canvas.style.width = width + 'px';
    render.canvas.style.height = height + 'px';

    clearAndRecreateBoundaries();
  }

  // Mouse drag
  const mouse = Mouse.create(render.canvas);
  const mouseConstraint = MouseConstraint.create(engine, {
    mouse,
    constraint: {
      stiffness: 0.2,
      render: { visible: false },
    },
  });
  Composite.add(world, mouseConstraint);
  render.mouse = mouse;

  // UI controls
  const spawnBtn = document.getElementById('spawnBtn');
  const clearBtn = document.getElementById('clearBtn');
  const pauseBtn = document.getElementById('pauseBtn');
  const gravitySlider = document.getElementById('gravitySlider');
  const gravityValue = document.getElementById('gravityValue');
  const explosionToolBtn = document.getElementById('explosionToolBtn');
  const explosionPower = document.getElementById('explosionPower');
  const explosionPowerValue = document.getElementById('explosionPowerValue');

  let paused = false;
  let explosionToolOn = false;

  spawnBtn.addEventListener('click', () => {
    const x = container.clientWidth * (0.3 + Math.random() * 0.4);
    const y = container.clientHeight * 0.25;
    createRagdoll(x, y, 1);
  });

  clearBtn.addEventListener('click', () => {
    for (const body of Array.from(ragdollBodies)) {
      Composite.remove(world, body);
      ragdollBodies.delete(body);
    }
  });

  pauseBtn.addEventListener('click', () => {
    paused = !paused;
    pauseBtn.textContent = paused ? 'Wznów' : 'Pauza';
  });

  gravitySlider.addEventListener('input', () => {
    const value = parseFloat(gravitySlider.value);
    world.gravity.y = value;
    gravityValue.textContent = value.toFixed(2);
  });

  explosionPower.addEventListener('input', () => {
    const value = parseFloat(explosionPower.value);
    explosionPowerValue.textContent = value.toFixed(3);
  });

  explosionToolBtn.addEventListener('click', () => {
    explosionToolOn = !explosionToolOn;
    explosionToolBtn.textContent = `Eksplozja: ${explosionToolOn ? 'wł.' : 'wył.'}`;
    explosionToolBtn.style.background = explosionToolOn ? '#3f466e' : '';
  });

  render.canvas.addEventListener('mousedown', (ev) => {
    if (!explosionToolOn) return;
    const rect = render.canvas.getBoundingClientRect();
    const x = (ev.clientX - rect.left) * (render.canvas.width / rect.width);
    const y = (ev.clientY - rect.top) * (render.canvas.height / rect.height);
    const worldX = x / pixelRatio;
    const worldY = y / pixelRatio;
    addExplosion(worldX, worldY, parseFloat(explosionPower.value));
  });

  // Timestep control
  (function tick() {
    if (!paused) {
      Engine.update(engine, 1000 / 60);
    }
    Render.world(render);
    requestAnimationFrame(tick);
  })();

  // Initial setup
  Render.run(render);
  Runner.run(runner, engine);
  fitRenderToContainer();
  window.addEventListener('resize', fitRenderToContainer);

  // Add some starter boxes to play with
  const starterStack = Composites.stack(120, 80, 6, 4, 0, 0, (x, y) => {
    const box = Bodies.rectangle(x, y, 28, 28, { restitution: 0.05, render: { fillStyle: '#48507d' } });
    return box;
  });
  Composite.add(world, starterStack);
})();
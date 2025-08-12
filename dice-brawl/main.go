package main

import (
    "encoding/json"
    "image/color"
    "log"
    "math"
    "math/rand"
    "os"
    "os/exec"
    "path/filepath"
    "strings"
    "time"

    "github.com/hajimehoshi/ebiten/v2"
    "github.com/hajimehoshi/ebiten/v2/ebitenutil"
    "github.com/hajimehoshi/ebiten/v2/inpututil"
)

const (
    screenWidth  = 480
    screenHeight = 320
)

type vector struct {
    x float64
    y float64
}

type rect struct {
    pos   vector
    size  vector
    vel   vector
    color color.Color
    alive bool
}

type gameState int

const (
    stateMenu gameState = iota
    statePlaying
    stateWin
    stateGameOver
    stateShop
)

type Game struct {
    player          rect
    enemies         []rect
    coins           int
    wave            int
    state           gameState
    attackTimer     float64
    lastUpdate      time.Time
    hasWeapon       bool
    weaponCost      int
    playerBaseColor color.Color
    modName         string
}

type ModConfig struct {
    WeaponCost  int      `json:"weaponCost"`
    StartCoins  int      `json:"startCoins"`
    PlayerColor []uint8  `json:"playerColor"`
}

func newPlayer() rect {
    return rect{
        pos:   vector{screenWidth/2 - 10, screenHeight/2 - 10},
        size:  vector{20, 20},
        vel:   vector{0, 0},
        color: color.RGBA{0x4c, 0xaf, 0x50, 0xff}, // default green
        alive: true,
    }
}

func newEnemy() rect {
    side := rand.Intn(4)
    var x, y float64
    switch side {
    case 0:
        x = 0
        y = float64(rand.Intn(screenHeight))
    case 1:
        x = screenWidth - 20
        y = float64(rand.Intn(screenHeight))
    case 2:
        x = float64(rand.Intn(screenWidth))
        y = 0
    default:
        x = float64(rand.Intn(screenWidth))
        y = screenHeight - 20
    }
    return rect{
        pos:   vector{x, y},
        size:  vector{18, 18},
        vel:   vector{0, 0},
        color: color.RGBA{0xe5, 0x39, 0x35, 0xff}, // red
        alive: true,
    }
}

func (g *Game) spawnWave(n int) {
    g.enemies = g.enemies[:0]
    for i := 0; i < n; i++ {
        g.enemies = append(g.enemies, newEnemy())
    }
}

func (r *rect) center() vector {
    return vector{r.pos.x + r.size.x/2, r.pos.y + r.size.y/2}
}

func clamp(v, min, max float64) float64 {
    if v < min {
        return min
    }
    if v > max {
        return max
    }
    return v
}

func (g *Game) Update() error {
    now := time.Now()
    if g.lastUpdate.IsZero() {
        g.lastUpdate = now
    }
    dt := now.Sub(g.lastUpdate).Seconds()
    g.lastUpdate = now

    switch g.state {
    case stateMenu:
        if ebiten.IsKeyPressed(ebiten.KeyEnter) || ebiten.IsMouseButtonPressed(ebiten.MouseButtonLeft) {
            g.reset()
            g.state = statePlaying
        }
        // Allow opening mod from menu
        g.handleModButton()
        return nil
    case stateWin:
        if ebiten.IsKeyPressed(ebiten.KeyEnter) {
            g.state = statePlaying
            g.wave++
            g.spawnWave(2 + g.wave)
        }
        g.handleModButton()
        return nil
    case stateGameOver:
        if ebiten.IsKeyPressed(ebiten.KeyEnter) {
            g.reset()
            g.state = statePlaying
        }
        g.handleModButton()
        return nil
    case stateShop:
        if ebiten.IsKeyPressed(ebiten.KeyEnter) {
            if !g.hasWeapon && g.coins >= g.weaponCost {
                g.coins -= g.weaponCost
                g.hasWeapon = true
            }
            g.state = statePlaying
        }
        if ebiten.IsKeyPressed(ebiten.KeyEscape) {
            g.state = statePlaying
        }
        g.handleModButton()
        return nil
    }

    // Controls
    moveSpeed := 120.0
    dashSpeed := 300.0
    isDashing := ebiten.IsKeyPressed(ebiten.KeySpace)

    // Open shop
    if ebiten.IsKeyPressed(ebiten.KeyB) {
        g.state = stateShop
        return nil
    }
    // Open mod dialog via key
    if ebiten.IsKeyPressed(ebiten.KeyM) {
        g.tryOpenModDialog()
    }

    // Click on MOD button
    g.handleModButton()

    var input vector
    if ebiten.IsKeyPressed(ebiten.KeyArrowLeft) || ebiten.IsKeyPressed(ebiten.KeyA) {
        input.x -= 1
    }
    if ebiten.IsKeyPressed(ebiten.KeyArrowRight) || ebiten.IsKeyPressed(ebiten.KeyD) {
        input.x += 1
    }
    if ebiten.IsKeyPressed(ebiten.KeyArrowUp) || ebiten.IsKeyPressed(ebiten.KeyW) {
        input.y -= 1
    }
    if ebiten.IsKeyPressed(ebiten.KeyArrowDown) || ebiten.IsKeyPressed(ebiten.KeyS) {
        input.y += 1
    }

    speed := moveSpeed
    if isDashing {
        speed = dashSpeed
        g.attackTimer += dt
    } else {
        g.attackTimer = 0
    }

    if input.x != 0 || input.y != 0 {
        length := math.Hypot(input.x, input.y)
        input.x /= length
        input.y /= length
    }

    g.player.pos.x += input.x * speed * dt
    g.player.pos.y += input.y * speed * dt
    g.player.pos.x = clamp(g.player.pos.x, 0, screenWidth-g.player.size.x)
    g.player.pos.y = clamp(g.player.pos.y, 0, screenHeight-g.player.size.y)

    // Enemies move towards player
    playerCenter := g.player.center()
    for i := range g.enemies {
        if !g.enemies[i].alive {
            continue
        }
        ec := g.enemies[i].center()
        dir := vector{playerCenter.x - ec.x, playerCenter.y - ec.y}
        dist := math.Hypot(dir.x, dir.y)
        if dist > 0 {
            dir.x /= dist
            dir.y /= dist
        }
        enemySpeed := 60.0 + float64(g.wave)*5
        g.enemies[i].pos.x += dir.x * enemySpeed * dt
        g.enemies[i].pos.y += dir.y * enemySpeed * dt

        if g.overlap(g.player, g.enemies[i]) {
            if isDashing || g.hasWeapon {
                g.enemies[i].alive = false
                g.coins += 1
            } else {
                g.state = stateGameOver
            }
        }
    }

    // Check win condition
    allDead := true
    for _, e := range g.enemies {
        if e.alive {
            allDead = false
            break
        }
    }
    if allDead {
        g.state = stateWin
    }

    return nil
}

func (g *Game) overlap(a, b rect) bool {
    if !a.alive || !b.alive {
        return false
    }
    return a.pos.x < b.pos.x+b.size.x && a.pos.x+a.size.x > b.pos.x && a.pos.y < b.pos.y+b.size.y && a.pos.y+a.size.y > b.pos.y
}

func (g *Game) Draw(screen *ebiten.Image) {
    screen.Fill(color.RGBA{0x12, 0x12, 0x16, 0xff})

    switch g.state {
    case stateMenu:
        ebitenutil.DebugPrint(screen, "Dice Brawl\nStrzałki/WASD: ruch\nSpacja: szarża (atak)\nB: sklep, M: mod\nENTER: start")
        g.drawModButton(screen)
        return
    case stateWin:
        ebitenutil.DebugPrint(screen, "Wygrana rundy!\n+1 coin za każdego pokonanego.\nB: sklep (broń "+itoa(g.weaponCost)+")\nM: mod\nENTER: kolejna fala")
        g.drawHUD(screen)
        g.drawModButton(screen)
        return
    case stateGameOver:
        ebitenutil.DebugPrint(screen, "Przegrana!\nENTER: spróbuj ponownie")
        g.drawHUD(screen)
        g.drawModButton(screen)
        return
    case stateShop:
        g.drawHUD(screen)
        msg := "SKLEP\n"
        if g.hasWeapon {
            msg += "Broń: posiadasz\n"
        } else {
            msg += "Broń: brak (" + itoa(g.weaponCost) + " coinów)\n"
        }
        msg += "ENTER: kup/wyjdź, ESC: wyjdź\nM: mod"
        ebitenutil.DebugPrintAt(screen, msg, 140, 100)
        g.drawModButton(screen)
        return
    }

    // Draw player
    baseColor := g.playerBaseColor
    if baseColor == nil {
        baseColor = color.RGBA{0x4c, 0xaf, 0x50, 0xff}
    }
    playerColor := baseColor
    if g.hasWeapon {
        playerColor = color.RGBA{0x21, 0x96, 0xf3, 0xff} // blue if armed
    }
    p := g.player
    p.color = playerColor
    drawRect(screen, p)

    // Draw enemies
    for _, e := range g.enemies {
        if e.alive {
            drawRect(screen, e)
        }
    }

    g.drawHUD(screen)
    g.drawModButton(screen)
}

func (g *Game) drawHUD(screen *ebiten.Image) {
    ebitenutil.DebugPrintAt(screen, "Coins: "+itoa(g.coins), 8, 8)
    ebitenutil.DebugPrintAt(screen, "Fala: "+itoa(g.wave+1), 8, 24)
    w := "NIE"
    if g.hasWeapon {
        w = "TAK"
    }
    ebitenutil.DebugPrintAt(screen, "Broń: "+w+" (B: sklep)", 8, 40)
    if g.modName != "" {
        ebitenutil.DebugPrintAt(screen, "Mod: "+g.modName, 8, 56)
    } else {
        ebitenutil.DebugPrintAt(screen, "Mod: brak (M: wybierz)", 8, 56)
    }
}

func drawRect(screen *ebiten.Image, r rect) {
    img := ebiten.NewImage(int(r.size.x), int(r.size.y))
    img.Fill(r.color)
    op := &ebiten.DrawImageOptions{}
    op.GeoM.Translate(r.pos.x, r.pos.y)
    screen.DrawImage(img, op)
}

func (g *Game) Layout(outsideWidth, outsideHeight int) (int, int) {
    return screenWidth, screenHeight
}

func (g *Game) reset() {
    g.player = newPlayer()
    g.enemies = nil
    g.coins = 0
    g.wave = 0
    g.hasWeapon = false
    g.weaponCost = 200
    g.playerBaseColor = color.RGBA{0x4c, 0xaf, 0x50, 0xff}
    g.modName = ""
    g.spawnWave(2)
}

func itoa(n int) string {
    if n == 0 {
        return "0"
    }
    neg := false
    if n < 0 {
        neg = true
        n = -n
    }
    buf := [20]byte{}
    i := len(buf)
    for n > 0 {
        i--
        buf[i] = byte('0' + (n % 10))
        n /= 10
    }
    if neg {
        i--
        buf[i] = '-'
    }
    return string(buf[i:])
}

// UI: simple MOD button in top-right
func (g *Game) modButtonRect() rect {
    return rect{pos: vector{screenWidth - 90, 8}, size: vector{80, 24}}
}

func (g *Game) drawModButton(screen *ebiten.Image) {
    r := g.modButtonRect()
    btn := rect{pos: r.pos, size: r.size, color: color.RGBA{0x44, 0x44, 0x55, 0xff}}
    drawRect(screen, btn)
    ebitenutil.DebugPrintAt(screen, "MOD", int(r.pos.x+26), int(r.pos.y+4))
}

func (g *Game) handleModButton() {
    if inpututil.IsMouseButtonJustPressed(ebiten.MouseButtonLeft) {
        mx, my := ebiten.CursorPosition()
        r := g.modButtonRect()
        if float64(mx) >= r.pos.x && float64(mx) <= r.pos.x+r.size.x && float64(my) >= r.pos.y && float64(my) <= r.pos.y+r.size.y {
            g.tryOpenModDialog()
        }
    }
}

func (g *Game) tryOpenModDialog() {
    // Determine Downloads dir
    downloads := g.detectDownloadsDir()

    // Try zenity / kdialog / yad / qarma
    var path string
    try := [][]string{
        {"zenity", "--file-selection", "--title=Wybierz mod", "--filename=" + filepath.ToSlash(downloads) + "/"},
        {"kdialog", "--getopenfilename", downloads, "*.json"},
        {"yad", "--file-selection", "--filename=" + filepath.ToSlash(downloads) + "/"},
        {"qarma", "--file-selection", "--filename=" + filepath.ToSlash(downloads) + "/"},
    }
    for _, cmd := range try {
        if _, err := exec.LookPath(cmd[0]); err == nil {
            out, err := exec.Command(cmd[0], cmd[1:]...).CombinedOutput()
            if err == nil {
                path = strings.TrimSpace(string(out))
                if path != "" {
                    break
                }
            }
        }
    }
    if path == "" {
        // Fallback: try to open file manager at Downloads, user can copy into game later
        _ = exec.Command("xdg-open", downloads).Start()
        return
    }
    g.loadMod(path)
}

func (g *Game) detectDownloadsDir() string {
    // xdg-user-dir DOWNLOAD
    if p, err := exec.Command("xdg-user-dir", "DOWNLOAD").Output(); err == nil {
        d := strings.TrimSpace(string(p))
        if d != "" {
            return d
        }
    }
    home, _ := os.UserHomeDir()
    if home == "" {
        return "."
    }
    return filepath.Join(home, "Downloads")
}

func (g *Game) loadMod(path string) {
    data, err := os.ReadFile(path)
    if err != nil {
        return
    }
    var cfg ModConfig
    if err := json.Unmarshal(data, &cfg); err != nil {
        return
    }
    if cfg.WeaponCost > 0 {
        g.weaponCost = cfg.WeaponCost
    }
    if cfg.StartCoins > 0 {
        g.coins = cfg.StartCoins
    }
    if len(cfg.PlayerColor) >= 3 {
        g.playerBaseColor = color.RGBA{cfg.PlayerColor[0], cfg.PlayerColor[1], cfg.PlayerColor[2], 0xff}
    }
    g.modName = filepath.Base(path)
}

func main() {
    rand.Seed(time.Now().UnixNano())

    g := &Game{state: stateMenu}

    ebiten.SetWindowSize(screenWidth*2, screenHeight*2)
    ebiten.SetWindowTitle("Dice Brawl")

    if err := ebiten.RunGame(g); err != nil {
        log.Fatal(err)
    }
}
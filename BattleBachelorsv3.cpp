// Battle Bachelors - 1v1 turn-based combat simulator

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

// ----- Entity: abstract base class for every Role -----
class Entity {
private:
    // Encapsulated stats
    string name;
    int maxHp, hp, str, maxSp, sp, speed;

    // Hidden damage roll position
    int dmgRollIndex;

    // Last skill this fighter used (-1 = none yet), needed by Force Inheritance
    int lastSkill;

    // Damage roll cycle: 1.00 -> 0.92 -> 0.85
    double nextRollMultiplier() {
        static const double ROLLS[3] = {1.00, 0.92, 0.85};
        double multiplier = ROLLS[dmgRollIndex];
        dmgRollIndex = (dmgRollIndex + 1) % 3;
        return multiplier;
    }

    // SP gain (capped at max SP)
    int gainSp(int amount) {
        int before = sp;
        sp = min(maxSp, sp + max(0, amount));
        return sp - before;
    }

    // SP spending
    bool spendSp(int cost) {
        if (cost > sp) return false;
        sp -= cost;
        return true;
    }

    // Strike: damage calculation + battle log + narrative
    int strike(Entity& target, double baseDamage, const string& move, const string& flavor) {
        double damage = baseDamage * nextRollMultiplier();
        cout << getLabel() << " used " << move << "!\n";
        cout << "  " << flavor << "\n";
        int dealt = target.takeDamage(static_cast<int>(floor(damage + 0.5)));
        cout << target.getLabel() << " lost " << dealt << " HP! ("
             << target.getHpPercent() << "% left)\n";
        if (!target.isAlive()) cout << target.getLabel() << " fainted!\n";
        return dealt;
    }

public:
    // Constructor
    Entity(const string& name, int hp, int str, int sp, int speed)
        : name(name), maxHp(max(1, hp)), hp(maxHp), str(max(0, str)),
          maxSp(max(0, sp)), sp(maxSp), speed(max(0, speed)), dmgRollIndex(0), lastSkill(-1) {}
    virtual ~Entity() {}

    // Role-specific interface (Polymorphism)
    virtual string getRoleName() const = 0;
    virtual string getAttackName() const = 0;
    virtual string getAttackDesc() const = 0;
    virtual string getAttackFlavor() const = 0;
    virtual double attackBaseDamage() const = 0;

    // Optional skills (default: none)
    virtual int getSkillCount() const { return 0; }
    virtual string getSkillName(int) const { return "-"; }
    virtual string getSkillDesc(int) const { return ""; }
    virtual string getSkillFlavor(int) const { return ""; }
    virtual int getSkillCost(int) const { return 0; }

    // Skill damage: the formula is owned by the role, but it reads the USER's stats
    // (so an inherited skill is calculated with the inheritor's stats)
    virtual double skillBaseDamage(int, const Entity&, const Entity&) const { return 0.0; }

    // Skill side effect (heal, recoil, inherit), called after the damage is dealt
    virtual void onSkillHit(int, Entity&, Entity&, int) {}

    // Force Inheritance support
    virtual bool isStealable(int) const { return true; }
    virtual void learnSkill(Entity&, int) {}

    // How attractive a skill is for the enemy AI (default: its damage)
    virtual double skillAiValue(int i, const Entity& foe) const {
        return skillBaseDamage(i, *this, foe);
    }

    // Action: basic attack
    void attack(Entity& target) {
        strike(target, attackBaseDamage(), getAttackName(), getAttackFlavor());
        gainSp(getAttackSpGain());
    }

    // Action: skill
    bool useSkill(int index, Entity& target) {
        if (index < 0 || index >= getSkillCount()) return false;
        if (!spendSp(getSkillCost(index))) return false;
        int dealt = strike(target, skillBaseDamage(index, *this, target),
                           getSkillName(index), getSkillFlavor(index));
        lastSkill = index;
        onSkillHit(index, *this, target, dealt);
        return true;
    }

    // Action: wait (Procrastinate)
    void wait() {
        int gained = gainSp(getWaitSpGain());
        cout << getLabel() << " procrastinated and caught a breath! (+" << gained << " SP)\n";
    }

    // Damage intake
    int takeDamage(int amount) {
        int before = hp;
        hp = max(0, hp - max(0, amount));
        return before - hp;
    }

    // ----- Controlled state changes used by skill effects -----
    // Healing (capped at max HP)
    int heal(int amount) {
        int before = hp;
        hp = min(maxHp, hp + max(0, amount));
        return hp - before;
    }
    // Self-damage (recoil can never knock the user out)
    int sacrificeHp(int amount) {
        int before = hp;
        hp = max(1, hp - max(0, amount));
        return before - hp;
    }

    // Reset to starting state
    virtual void reset() {
        hp = maxHp;
        sp = maxSp;
        dmgRollIndex = 0;
        lastSkill = -1;
    }

    // State checks
    bool isAlive() const { return hp > 0; }
    bool canAfford(int skill) const { return sp >= getSkillCost(skill); }
    bool canWait() const { return getSkillCount() > 0; }

    // Fixed values
    static int getAttackSpGain() { return 1; }
    static int getWaitSpGain() { return 3; }

    // Getters
    string getName() const { return name; }
    string getLabel() const { return name + "'s " + getRoleName(); }
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getSp() const { return sp; }
    int getMaxSp() const { return maxSp; }
    int getStr() const { return str; }
    int getSpeed() const { return speed; }
    int getLastSkill() const { return lastSkill; }

    // HP percentage for display
    int getHpPercent() const {
        int percent = static_cast<int>(floor(100.0 * hp / maxHp + 0.5));
        if (hp > 0 && percent < 1) percent = 1;
        return percent;
    }
};

// ----- Roles -----

// Information Engineer
class InformationEngineer : public Entity {
private:
    // Inherited skill slot (filled once by Force Inheritance, kept for the whole match)
    Entity* inheritedFrom;
    int inheritedIndex;

    bool hasInherited() const { return inheritedFrom != nullptr; }

public:
    explicit InformationEngineer(const string& name)
        : Entity(name, 280, 45, 9, 24), inheritedFrom(nullptr), inheritedIndex(-1) {}

    // Role info and basic attack
    string getRoleName() const override { return "Information Engineer"; }
    string getAttackName() const override { return "Packet Injection"; }
    string getAttackDesc() const override { return "Minor damage."; }
    string getAttackFlavor() const override {
        return "A rapid data packet probes the enemy's firewall and builds processing power.";
    }
    double attackBaseDamage() const override { return getStr() * 0.9; }

    // Skills (slot 1 turns into the inherited skill after Force Inheritance)
    int getSkillCount() const override { return 2; }
    string getSkillName(int i) const override {
        if (i == 0) return "Buffer Overflow";
        return hasInherited() ? inheritedFrom->getSkillName(inheritedIndex) + " (Inherited)"
                              : "Force Inheritance";
    }
    string getSkillDesc(int i) const override {
        if (i == 0) return "Moderate damage + bonus damage for every SP the target currently has.";
        if (hasInherited())
            return "Inherited skill. Uses YOUR stats with the original SP cost. "
                   + inheritedFrom->getSkillDesc(inheritedIndex);
        return "Minimal damage. Copies the target's last used skill for the rest of the match (one time only).";
    }
    string getSkillFlavor(int i) const override {
        if (i == 0) return "Junk data floods the enemy's memory. The more SP it holds, the worse the crash!";
        if (hasInherited())
            return "The IE runs the inherited method as its own! " + inheritedFrom->getSkillFlavor(inheritedIndex);
        return "Bypassing encapsulation, the IE forcefully inherits the enemy's private methods!";
    }
    int getSkillCost(int i) const override {
        if (i == 1 && hasInherited()) return inheritedFrom->getSkillCost(inheritedIndex);
        return 3;
    }
    double skillBaseDamage(int i, const Entity& user, const Entity& target) const override {
        if (i == 0) return user.getStr() * (1.0 + 0.1 * target.getSp());
        if (hasInherited()) return inheritedFrom->skillBaseDamage(inheritedIndex, user, target);
        return user.getStr() * 0.4;
    }

    // Force Inheritance: inherit once. Inherited skill: run the original effect.
    void onSkillHit(int i, Entity& user, Entity& target, int dealt) override {
        if (i != 1) return;
        if (hasInherited()) {
            inheritedFrom->onSkillHit(inheritedIndex, user, target, dealt);
            return;
        }
        int last = target.getLastSkill();
        if (last >= 0 && target.isStealable(last)) {
            user.learnSkill(target, last);
            cout << user.getLabel() << " inherited " << target.getSkillName(last) << "!\n";
        } else {
            cout << "But there was nothing to inherit!\n";
        }
    }

    // Only Buffer Overflow can be inherited from an IE (prevents inheritance loops)
    bool isStealable(int i) const override { return i == 0; }
    void learnSkill(Entity& from, int index) override {
        inheritedFrom = &from;
        inheritedIndex = index;
    }

    // AI: Force Inheritance is only worth it when there is something to inherit
    double skillAiValue(int i, const Entity& foe) const override {
        if (i == 1 && !hasInherited()) {
            int last = foe.getLastSkill();
            if (last < 0 || !foe.isStealable(last)) return 0.0;
            return 0.7 * foe.skillBaseDamage(last, *this, foe);
        }
        return skillBaseDamage(i, *this, foe);
    }

    void reset() override {
        Entity::reset();
        inheritedFrom = nullptr;
        inheritedIndex = -1;
    }
};

// Electrical Engineer
class ElectricalEngineer : public Entity {
public:
    explicit ElectricalEngineer(const string& name) : Entity(name, 330, 40, 7, 10) {}

    // Role info and basic attack
    string getRoleName() const override { return "Electrical Engineer"; }
    string getAttackName() const override { return "Terminal Strike"; }
    string getAttackDesc() const override { return "Minor physical damage."; }
    string getAttackFlavor() const override {
        return "A heavy, grounded blow with an insulated wrench. The impact builds a small charge.";
    }
    double attackBaseDamage() const override { return getStr() * 0.8; }

    // Skills
    int getSkillCount() const override { return 2; }
    string getSkillName(int i) const override { return i == 0 ? "Capacitor Overload" : "Short Circuit"; }
    string getSkillDesc(int i) const override {
        return i == 0 ? "Heavy damage that grows as your own HP gets lower."
                      : "Reliable mid damage.";
    }
    string getSkillFlavor(int i) const override {
        return i == 0 ? "Life support is rerouted into offense as stored energy bursts out of control!"
                      : "A grounding fault hits the enemy's hardware with a sharp surge!";
    }
    int getSkillCost(int i) const override { return i == 0 ? 3 : 2; }
    double skillBaseDamage(int i, const Entity& user, const Entity&) const override {
        if (i == 0) {
            double missingHp = 1.0 - static_cast<double>(user.getHp()) / user.getMaxHp();
            return user.getStr() * (1.0 + missingHp);
        }
        return user.getStr() * 1.1;
    }
};

// Biomedic Engineer
class BiomedicEngineer : public Entity {
public:
    explicit BiomedicEngineer(const string& name) : Entity(name, 240, 40, 8, 16) {}

    // Role info and basic attack
    string getRoleName() const override { return "Biomedic Engineer"; }
    string getAttackName() const override { return "Cellular Tap"; }
    string getAttackDesc() const override { return "Minor damage."; }
    string getAttackFlavor() const override {
        return "A precise syringe jab extracts just enough biological data to generate power.";
    }
    double attackBaseDamage() const override { return getStr() * 0.6; }

    // Skills
    int getSkillCount() const override { return 2; }
    string getSkillName(int i) const override { return i == 0 ? "Biomaterial Siphon" : "Neural Interface Overdrive"; }
    string getSkillDesc(int i) const override {
        return i == 0 ? "Moderate damage. Heals you for 50% of the damage dealt."
                      : "Massive damage.";
    }
    string getSkillFlavor(int i) const override {
        return i == 0 ? "Raw organic matter is harvested from the target to patch up the user's tissue."
                      : "The neural link is pushed past safe limits and a bio-electric pulse annihilates the target!";
    }
    int getSkillCost(int i) const override { return i == 0 ? 3 : 7; }
    double skillBaseDamage(int i, const Entity& user, const Entity&) const override {
        return user.getStr() * (i == 0 ? 1.3 : 2.2);
    }

    // Siphon: heal 50% of damage dealt
    void onSkillHit(int skill, Entity& user, Entity&, int dealt) override {
        if (skill != 0) return;
        int healed = user.heal(dealt / 2);
        if (healed > 0)
            cout << user.getLabel() << " siphoned biomaterial and restored " << healed << " HP!\n";
    }
};

// Freshman
class Freshman : public Entity {
public:
    explicit Freshman(const string& name) : Entity(name, 330, 45, 5, 8) {}

    // Role info and basic attack
    string getRoleName() const override { return "Freshman"; }
    string getAttackName() const override { return "Last-Minute Cram"; }
    string getAttackDesc() const override { return "Very low damage."; }
    string getAttackFlavor() const override {
        return "Skimming a textbook while swatting at the enemy without looking. The stress builds energy.";
    }
    double attackBaseDamage() const override { return getStr() * 0.7; }

    // Skill
    int getSkillCount() const override { return 1; }
    string getSkillName(int) const override { return "All-Nighter"; }
    string getSkillDesc(int) const override {
        return "High damage. You take 20% of your max HP as recoil.";
    }
    string getSkillFlavor(int) const override {
        return "Fueled by panic and looming deadlines, the reckless attack hits hard. Then comes the crash.";
    }
    int getSkillCost(int) const override { return 4; }
    double skillBaseDamage(int, const Entity& user, const Entity&) const override {
        return user.getStr() * 2.5;
    }

    // All-Nighter: recoil
    void onSkillHit(int, Entity& user, Entity&, int) override {
        int recoil = user.sacrificeHp(user.getMaxHp() / 5);
        cout << user.getLabel() << " took " << recoil << " recoil damage!\n";
    }
};

// ----- Input and display helpers -----

// Number input with validation
static int readChoice(int minValue, int maxValue) {
    int failures = 0;
    while (true) {
        cout << "> ";
        int choice;
        if (cin >> choice) {
            cin.ignore(1000, '\n');
            failures = 0;
            if (choice >= minValue && choice <= maxValue) return choice;
            cout << "Please choose a number from " << minValue << " to " << maxValue << ".\n";
        } else {
            if (cin.eof()) throw "Input closed.";
            if (++failures > 20) throw "Input is not available. Please run the game from a terminal.";
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Please enter a number from " << minValue << " to " << maxValue << ".\n";
        }
    }
}

// Pause between actions
static void waitForEnter() {
    cout << "\n[Press Enter to continue]";
    string line;
    if (!getline(cin, line)) throw "Input closed.";
    cout << "\n";
}

// Progress bar
static string makeBar(int current, int maximum, int width) {
    int filled = 0;
    if (maximum > 0) filled = static_cast<int>(floor(static_cast<double>(width) * current / maximum + 0.5));
    if (current > 0 && filled < 1) filled = 1;
    return "[" + string(filled, '#') + string(width - filled, '-') + "]";
}

// ----- BattleManager: runs one 1v1 battle (Abstraction) -----
class BattleManager {
private:
    // Fighters
    Entity& player;
    Entity& enemy;

    // Status panel
    void printStatus(int turn) const {
        cout << "\n================= Turn " << turn << " =================\n";
        cout << left;
        const Entity* fighters[2] = {&player, &enemy};
        for (int i = 0; i < 2; ++i) {
            const Entity& e = *fighters[i];
            cout << e.getLabel() << "\n";
            cout << "  HP " << makeBar(e.getHp(), e.getMaxHp(), 20) << " "
                 << setw(5) << (to_string(e.getHpPercent()) + "%")
                 << setw(10) << ("(" + to_string(e.getHp()) + "/" + to_string(e.getMaxHp()) + ")");
            if (e.getMaxSp() > 0)
                cout << " SP " << makeBar(e.getSp(), e.getMaxSp(), e.getMaxSp())
                     << " " << e.getSp() << "/" << e.getMaxSp();
            else
                cout << " SP -";
            cout << "  SPD " << e.getSpeed() << "\n";
        }
        cout << "======================================================\n\n";
    }

    // Player turn: action menu (every option shows what it does)
    void playerTurn(Entity& actor, Entity& foe) {
        while (true) {
            int skills = actor.getSkillCount();
            cout << "What will " << actor.getLabel() << " do?\n";
            cout << "  1. " << actor.getAttackName()
                 << " (+" << Entity::getAttackSpGain() << " SP)\n"
                 << "       " << actor.getAttackDesc() << "\n";
            for (int i = 0; i < skills; ++i) {
                cout << "  " << i + 2 << ". " << actor.getSkillName(i)
                     << " (" << actor.getSkillCost(i) << " SP)";
                if (!actor.canAfford(i)) cout << "  - not enough SP";
                cout << "\n       " << actor.getSkillDesc(i) << "\n";
            }
            int waitOption = actor.canWait() ? skills + 2 : 0;
            if (waitOption != 0)
                cout << "  " << waitOption << ". Wait / Procrastinate (+" << Entity::getWaitSpGain() << " SP)\n"
                     << "       Skip your turn to recover SP.\n";

            int choice = readChoice(1, waitOption != 0 ? waitOption : skills + 1);
            cout << "\n";
            if (choice == 1) { actor.attack(foe); return; }
            if (choice == waitOption) { actor.wait(); return; }
            if (actor.useSkill(choice - 2, foe)) return;
            cout << "Not enough SP!\n\n";
        }
    }

    // Enemy turn: rule-based AI
    void enemyTurn(Entity& actor, Entity& foe) {
        // Rule 1: finish with basic attack if it KOs
        if (actor.attackBaseDamage() >= foe.getHp()) { actor.attack(foe); return; }

        // Rules 2-3: best affordable skill (KO first, then highest AI value)
        int best = -1;
        bool bestKo = false;
        double bestValue = 0.0;
        for (int i = 0; i < actor.getSkillCount(); ++i) {
            if (!actor.canAfford(i)) continue;
            double value = actor.skillAiValue(i, foe);
            if (value <= 0.0) continue;
            bool ko = actor.skillBaseDamage(i, actor, foe) >= foe.getHp();
            if (best == -1 || (ko && !bestKo) || (ko == bestKo && value > bestValue)) {
                best = i; bestKo = ko; bestValue = value;
            }
        }
        if (best != -1) { actor.useSkill(best, foe); return; }

        // Rule 4: wait for SP if a reachable skill would KO
        for (int i = 0; i < actor.getSkillCount(); ++i) {
            if (actor.getSkillCost(i) <= actor.getMaxSp() &&
                actor.skillBaseDamage(i, actor, foe) >= foe.getHp()) { actor.wait(); return; }
        }

        // Rule 5: default basic attack
        actor.attack(foe);
    }

public:
    // Constructor
    BattleManager(Entity& player, Entity& enemy) : player(player), enemy(enemy) {}

    // Battle loop
    bool run() {
        // Setup
        player.reset();
        enemy.reset();
        cout << "\nBattle started between " << player.getName()
             << " and " << enemy.getName() << "!\n";

        int turn = 0;
        while (player.isAlive() && enemy.isAlive()) {
            ++turn;
            printStatus(turn);

            // Turn order: speed, ties alternate
            bool playerFirst;
            if (player.getSpeed() != enemy.getSpeed()) playerFirst = player.getSpeed() > enemy.getSpeed();
            else playerFirst = (turn % 2 == 1);

            // Actions
            for (int step = 0; step < 2 && player.isAlive() && enemy.isAlive(); ++step) {
                bool playerActs = (step == 0) == playerFirst;
                if (playerActs) playerTurn(player, enemy);
                else enemyTurn(enemy, player);

                if (player.isAlive() && enemy.isAlive()) waitForEnter();
            }
        }

        // Result
        bool won = player.isAlive();
        cout << "\n=======================================\n"
             << "Winner: " << (won ? player : enemy).getName()
             << "  (" << turn << " turns)\n"
             << (won ? "You won the battle!" : "You lost the battle...") << "\n";

        // Cleanup: fresh state for the role menu
        player.reset();
        enemy.reset();
        return won;
    }
};

// ----- Role selection menu (Polymorphism: vector<Entity*>) -----
static int chooseRole(const vector<Entity*>& roster, const string& prompt) {
    cout << "\n" << prompt << "\n";
    for (size_t i = 0; i < roster.size(); ++i) {
        const Entity& r = *roster[i];
        cout << "  " << i + 1 << ". " << r.getRoleName() << "\n"
             << "     Skills: ";
        if (r.getSkillCount() == 0) cout << "None";
        for (int s = 0; s < r.getSkillCount(); ++s) {
            if (s > 0) cout << ", ";
            cout << r.getSkillName(s);
        }
        cout << "\n";
    }
    return readChoice(1, static_cast<int>(roster.size())) - 1;
}

// ----- Main -----
int main() {
    // Role objects
    InformationEngineer heroIE("Player"), enemyIE("Foe");
    ElectricalEngineer heroEE("Player"), enemyEE("Foe");
    BiomedicEngineer heroBE("Player"), enemyBE("Foe");
    Freshman heroFreshman("Player"), enemyFreshman("Foe");

    // Role lists
    vector<Entity*> heroes, enemies;
    heroes.push_back(&heroIE);        enemies.push_back(&enemyIE);
    heroes.push_back(&heroEE);        enemies.push_back(&enemyEE);
    heroes.push_back(&heroBE);        enemies.push_back(&enemyBE);
    heroes.push_back(&heroFreshman);  enemies.push_back(&enemyFreshman);

    // Game loop
    try {
        cout << "=== BATTLE BACHELORS ===\n";
        int again = 1;
        while (again == 1) {
            int heroIndex = chooseRole(heroes, "Choose your Role:");
            int enemyIndex = chooseRole(enemies, "Choose your opponent's Role:");

            BattleManager battle(*heroes[heroIndex], *enemies[enemyIndex]);
            battle.run();

            cout << "\nPlay again?\n  1. Yes\n  2. No\n";
            again = readChoice(1, 2);
        }
    } catch (const char* message) {
        cout << "\n" << message << "\n";
        return 1;
    }
    return 0;
}


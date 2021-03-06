#include "player.hpp"

#include <stdexcept>
#include <iostream>

#include <components/esm/esmreader.hpp>
#include <components/esm/esmwriter.hpp>
#include <components/esm/player.hpp>
#include <components/esm/defs.hpp>
#include <components/esm/loadbsgn.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "class.hpp"
#include "ptr.hpp"
#include "cellstore.hpp"

namespace MWWorld
{
    Player::Player (const ESM::NPC *player)
      : mCellStore(0),
        mLastKnownExteriorPosition(0,0,0),
        mMarkedCell(NULL),
        mAutoMove(false),
        mForwardBackward(0),
        mTeleported(false),
        mCurrentCrimeId(-1),
        mPaidCrimeId(-1),
        mAttackingOrSpell(false)
    {
        ESM::CellRef cellRef;
        cellRef.blank();
        cellRef.mRefID = "player";
        mPlayer = LiveCellRef<ESM::NPC>(cellRef, player);

        ESM::Position playerPos = mPlayer.mData.getPosition();
        playerPos.pos[0] = playerPos.pos[1] = playerPos.pos[2] = 0;
        mPlayer.mData.setPosition(playerPos);
    }

    void Player::saveSkillsAttributes()
    {
        MWMechanics::NpcStats& stats = getPlayer().getClass().getNpcStats(getPlayer());
        for (int i=0; i<ESM::Skill::Length; ++i)
            mSaveSkills[i] = stats.getSkill(i);
        for (int i=0; i<ESM::Attribute::Length; ++i)
            mSaveAttributes[i] = stats.getAttribute(i);
    }

    void Player::restoreSkillsAttributes()
    {
        MWMechanics::NpcStats& stats = getPlayer().getClass().getNpcStats(getPlayer());
        for (int i=0; i<ESM::Skill::Length; ++i)
            stats.setSkill(i, mSaveSkills[i]);
        for (int i=0; i<ESM::Attribute::Length; ++i)
            stats.setAttribute(i, mSaveAttributes[i]);
    }

    void Player::setWerewolfSkillsAttributes()
    {
        const MWWorld::Store<ESM::GameSetting>& gmst = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();
        MWMechanics::NpcStats& stats = getPlayer().getClass().getNpcStats(getPlayer());
        for(size_t i = 0;i < ESM::Attribute::Length;++i)
        {
            // Oh, Bethesda. It's "Intelligence".
            std::string name = "fWerewolf"+((i==ESM::Attribute::Intelligence) ? std::string("Intellegence") :
                                            ESM::Attribute::sAttributeNames[i]);

            MWMechanics::AttributeValue value = stats.getAttribute(i);
            value.setBase(int(gmst.find(name)->getFloat()));
            stats.setAttribute(i, value);
        }

        for(size_t i = 0;i < ESM::Skill::Length;i++)
        {
            // Acrobatics is set separately for some reason.
            if(i == ESM::Skill::Acrobatics)
                continue;

            // "Mercantile"! >_<
            std::string name = "fWerewolf"+((i==ESM::Skill::Mercantile) ? std::string("Merchantile") :
                                            ESM::Skill::sSkillNames[i]);

            MWMechanics::SkillValue value = stats.getSkill(i);
            value.setBase(int(gmst.find(name)->getFloat()));
            stats.setSkill(i, value);
        }
    }

    void Player::set(const ESM::NPC *player)
    {
        mPlayer.mBase = player;
    }

    void Player::setCell (MWWorld::CellStore *cellStore)
    {
        mCellStore = cellStore;
    }

    MWWorld::Ptr Player::getPlayer()
    {
        MWWorld::Ptr ptr (&mPlayer, mCellStore);
        return ptr;
    }

    void Player::setBirthSign (const std::string &sign)
    {
        mSign = sign;
    }

    const std::string& Player::getBirthSign() const
    {
        return mSign;
    }

    void Player::setDrawState (MWMechanics::DrawState_ state)
    {
         MWWorld::Ptr ptr = getPlayer();
         ptr.getClass().getNpcStats(ptr).setDrawState (state);
    }

    bool Player::getAutoMove() const
    {
        return mAutoMove;
    }

    void Player::setAutoMove (bool enable)
    {
        MWWorld::Ptr ptr = getPlayer();

        mAutoMove = enable;

        int value = mForwardBackward;

        if (mAutoMove)
            value = 1;

        ptr.getClass().getMovementSettings(ptr).mPosition[1] = static_cast<float>(value);
    }

    void Player::setLeftRight (int value)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getMovementSettings(ptr).mPosition[0] = static_cast<float>(value);
    }

    void Player::setForwardBackward (int value)
    {
        MWWorld::Ptr ptr = getPlayer();

        mForwardBackward = value;

        if (mAutoMove)
            value = 1;

        ptr.getClass().getMovementSettings(ptr).mPosition[1] = static_cast<float>(value);
    }

    void Player::setUpDown(int value)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getMovementSettings(ptr).mPosition[2] = static_cast<float>(value);
    }

    void Player::setRunState(bool run)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getCreatureStats(ptr).setMovementFlag(MWMechanics::CreatureStats::Flag_Run, run);
    }

    void Player::setSneak(bool sneak)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getCreatureStats(ptr).setMovementFlag(MWMechanics::CreatureStats::Flag_Sneak, sneak);
    }

    void Player::yaw(float yaw)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getMovementSettings(ptr).mRotation[2] += yaw;
    }
    void Player::pitch(float pitch)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getMovementSettings(ptr).mRotation[0] += pitch;
    }
    void Player::roll(float roll)
    {
        MWWorld::Ptr ptr = getPlayer();
        ptr.getClass().getMovementSettings(ptr).mRotation[1] += roll;
    }

    MWMechanics::DrawState_ Player::getDrawState()
    {
         MWWorld::Ptr ptr = getPlayer();
         return ptr.getClass().getNpcStats(ptr).getDrawState();
    }

    void Player::activate()
    {
        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
            return;

        MWWorld::Ptr player = getPlayer();
        const MWMechanics::NpcStats &playerStats = player.getClass().getNpcStats(player);
        if (playerStats.isParalyzed() || playerStats.getKnockedDown())
            return;

        MWWorld::Ptr toActivate = MWBase::Environment::get().getWorld()->getFacedObject();

        if (toActivate.isEmpty())
            return;

        if (toActivate.getClass().getName(toActivate) == "") // objects without name presented to user can never be activated
            return;

        if (toActivate.getClass().isActor())
        {
            MWMechanics::CreatureStats &stats = toActivate.getClass().getCreatureStats(toActivate);

            if (stats.getAiSequence().isInCombat() && !stats.isDead())
                return;
        }

        MWBase::Environment::get().getWorld()->activate(toActivate, player);
    }

    bool Player::wasTeleported() const
    {
        return mTeleported;
    }

    void Player::setTeleported(bool teleported)
    {
        mTeleported = teleported;
    }

    void Player::setAttackingOrSpell(bool attackingOrSpell)
    {
        mAttackingOrSpell = attackingOrSpell;
    }

    bool Player::getAttackingOrSpell() const
    {
        return mAttackingOrSpell;
    }

    bool Player::isInCombat() {
        return MWBase::Environment::get().getMechanicsManager()->getActorsFighting(getPlayer()).size() != 0;
    }

    void Player::markPosition(CellStore *markedCell, ESM::Position markedPosition)
    {
        mMarkedCell = markedCell;
        mMarkedPosition = markedPosition;
    }

    void Player::getMarkedPosition(CellStore*& markedCell, ESM::Position &markedPosition) const
    {
        markedCell = mMarkedCell;
        if (mMarkedCell)
            markedPosition = mMarkedPosition;
    }

    void Player::clear()
    {
        mCellStore = 0;
        mSign.clear();
        mMarkedCell = 0;
        mAutoMove = false;
        mForwardBackward = 0;
        mTeleported = false;
    }

    void Player::write (ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        ESM::Player player;

        mPlayer.save (player.mObject);
        player.mCellId = mCellStore->getCell()->getCellId();

        player.mCurrentCrimeId = mCurrentCrimeId;
        player.mPaidCrimeId = mPaidCrimeId;

        player.mBirthsign = mSign;

        player.mLastKnownExteriorPosition[0] = mLastKnownExteriorPosition.x();
        player.mLastKnownExteriorPosition[1] = mLastKnownExteriorPosition.y();
        player.mLastKnownExteriorPosition[2] = mLastKnownExteriorPosition.z();

        if (mMarkedCell)
        {
            player.mHasMark = true;
            player.mMarkedPosition = mMarkedPosition;
            player.mMarkedCell = mMarkedCell->getCell()->getCellId();
        }
        else
            player.mHasMark = false;

        player.mAutoMove = mAutoMove ? 1 : 0;

        for (int i=0; i<ESM::Attribute::Length; ++i)
            mSaveAttributes[i].writeState(player.mSaveAttributes[i]);
        for (int i=0; i<ESM::Skill::Length; ++i)
            mSaveSkills[i].writeState(player.mSaveSkills[i]);

        writer.startRecord (ESM::REC_PLAY);
        player.save (writer);
        writer.endRecord (ESM::REC_PLAY);
    }

    bool Player::readRecord (ESM::ESMReader& reader, uint32_t type)
    {
        if (type==ESM::REC_PLAY)
        {
            ESM::Player player;
            player.load (reader);

            if (!mPlayer.checkState (player.mObject))
            {
                // this is the one object we can not silently drop.
                throw std::runtime_error ("invalid player state record (object state)");
            }

            mPlayer.load (player.mObject);

            for (int i=0; i<ESM::Attribute::Length; ++i)
                mSaveAttributes[i].readState(player.mSaveAttributes[i]);
            for (int i=0; i<ESM::Skill::Length; ++i)
                mSaveSkills[i].readState(player.mSaveSkills[i]);

            if (player.mObject.mNpcStats.mWerewolfDeprecatedData && player.mObject.mNpcStats.mIsWerewolf)
            {
                saveSkillsAttributes();
                setWerewolfSkillsAttributes();
            }

            getPlayer().getClass().getCreatureStats(getPlayer()).getAiSequence().clear();

            MWBase::World& world = *MWBase::Environment::get().getWorld();

            try
            {
                mCellStore = world.getCell (player.mCellId);
            }
            catch (...)
            {
                std::cerr << "Player cell '" << player.mCellId.mWorldspace << "' no longer exists" << std::endl;
                // Cell no longer exists. Place the player in a default cell.
                ESM::Position pos = mPlayer.mData.getPosition();
                MWBase::Environment::get().getWorld()->indexToPosition(0, 0, pos.pos[0], pos.pos[1], true);
                pos.pos[2] = 0;
                mPlayer.mData.setPosition(pos);
                mCellStore = world.getExterior(0,0);
            }

            if (!player.mBirthsign.empty())
            {
                const ESM::BirthSign* sign = world.getStore().get<ESM::BirthSign>().search (player.mBirthsign);
                if (!sign)
                    throw std::runtime_error ("invalid player state record (birthsign does not exist)");

                // To handle the case where a birth sign was edited in between play sessions (does not yet handle removing the old spells)
                // Also needed for ess-imported savegames which do not specify the birtsign spells in the player's spell list.
                for (std::vector<std::string>::const_iterator iter (sign->mPowers.mList.begin());
                    iter!=sign->mPowers.mList.end(); ++iter)
                {
                    getPlayer().getClass().getCreatureStats(getPlayer()).getSpells().add (*iter);
                }
            }

            mCurrentCrimeId = player.mCurrentCrimeId;
            mPaidCrimeId = player.mPaidCrimeId;

            mSign = player.mBirthsign;

            mLastKnownExteriorPosition.x() = player.mLastKnownExteriorPosition[0];
            mLastKnownExteriorPosition.y() = player.mLastKnownExteriorPosition[1];
            mLastKnownExteriorPosition.z() = player.mLastKnownExteriorPosition[2];

            if (player.mHasMark && !player.mMarkedCell.mPaged)
            {
                // interior cell -> need to check if it exists (exterior cell will be
                // generated on the fly)

                if (!world.getStore().get<ESM::Cell>().search (player.mMarkedCell.mWorldspace))
                    player.mHasMark = false; // drop mark silently
            }

            if (player.mHasMark)
            {
                mMarkedPosition = player.mMarkedPosition;
                mMarkedCell = world.getCell (player.mMarkedCell);
            }
            else
            {
                mMarkedCell = 0;
            }

            mAutoMove = player.mAutoMove!=0;

            mForwardBackward = 0;
            mTeleported = false;

            return true;
        }

        return false;
    }

    int Player::getNewCrimeId()
    {
        return ++mCurrentCrimeId;
    }

    void Player::recordCrimeId()
    {
        mPaidCrimeId = mCurrentCrimeId;
    }

    int Player::getCrimeId() const
    {
        return mPaidCrimeId;
    }
}

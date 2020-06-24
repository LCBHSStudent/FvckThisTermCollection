#include "PokemonSkill.h"
#include "PokemonBase.h"

#define REGISTER_SKILL REGISTER_METHOD

#define JUDGE_ATTR(_ATTR) \
    (dest->m_pkmAttr == PokemonBase::PokemonAttribute::_ATTR)

#define ATK_DEBUG(_SKILL) \
    std::cout << user->m_name << "ʹ��" #_SKILL "������" << dest->m_name << "\n"

QHash<QString, PokemonSkill::SkillFunc> PokemonSkill::s_skillMap = {};

REGISTER_SKILL(FireBall) {
    ATK_DEBUG("����");
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
    
}

REGISTER_SKILL(GreassLeaf) {
    ATK_DEBUG("Ҷ��");
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}

REGISTER_SKILL(WaterBullet) {
    ATK_DEBUG("ˮ��");
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}

REGISTER_SKILL(WindBreath) {
    ATK_DEBUG("��Ϣ");
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}

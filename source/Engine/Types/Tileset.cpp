#include <Engine/Types/Tileset.h>
#include <Engine/Utilities/StringUtils.h>

Tileset::Tileset(ISprite* sprite, int tileWidth, int tileHeight, size_t firstgid, size_t startTile, size_t tileCount, char* filename) {
    if (sprite->Spritesheets.size() < 1)
        return;

    Sprite = sprite;
    NumCols = Sprite->Spritesheets[0]->Width / tileWidth;
    NumRows = Sprite->Spritesheets[0]->Height / tileHeight;
    TileWidth = tileWidth;
    TileHeight = tileHeight;
    TileCount = tileCount;
    StartTile = startTile;
    FirstGlobalTileID = firstgid;
    Filename = StringUtils::Duplicate(filename);
}

void Tileset::RunAnimations() {
    for (map<int, TileAnimator>::iterator it = AnimatorMap.begin(); it != AnimatorMap.end(); it++) {
        TileAnimator& animator = it->second;
        if (!animator.Paused)
            animator.Animate();
    }
}

void Tileset::RestartAnimations() {
    for (map<int, TileAnimator>::iterator it = AnimatorMap.begin(); it != AnimatorMap.end(); it++) {
        TileAnimator& animator = it->second;
        animator.RestartAnimation();
        animator.Paused = false;
    }
}

void Tileset::AddTileAnimSequence(int tileID, TileSpriteInfo* tileSpriteInfo, vector<int>& tileIDs, vector<int>& durations) {
    ISprite* tileSprite = Sprite;
    if (!tileSprite)
        return;

    if (tileSprite->Animations.size() == 1)
        tileSprite->AddAnimation("TileAnimation", 1, 0);

    size_t animID = tileSprite->Animations.size() - 1;

    tileSprite->RemoveFrames(animID);

    if (!tileIDs.size()) {
        AnimatorMap.erase(tileID);
        return;
    }

    if (!NumCols)
        return;

    for (size_t i = 0; i < tileIDs.size(); i++) {
        int otherTileID = tileIDs[i];
        tileSprite->AddFrame(animID, durations[i],
            (otherTileID % NumCols) * TileWidth,
            (otherTileID / NumCols) * TileHeight,
            TileWidth, TileHeight, -TileWidth / 2, -TileHeight / 2, 0);
    }

    tileSprite->RefreshGraphicsID();

    TileAnimator animator(tileSpriteInfo, tileSprite, animID);
    animator.RestartAnimation();

    AnimatorMap.insert( { tileID, animator } );
}

void Tileset::AddTileAnimSequence(int tileID, TileSpriteInfo* tileSpriteInfo, ISprite* animSprite, int animID) {
    if (animSprite == nullptr) {
        AnimatorMap.erase(tileID);
        return;
    }

    TileAnimator animator(tileSpriteInfo, animSprite, animID);
    animator.RestartAnimation();

    AnimatorMap.insert( { tileID, animator } );
}

TileAnimator* Tileset::GetTileAnimSequence(int tileID) {
    std::map<int, TileAnimator>::iterator it = AnimatorMap.find(tileID);
    if (it == AnimatorMap.end())
        return nullptr;

    return &it->second;
}

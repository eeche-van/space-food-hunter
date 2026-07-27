#include <stdio.h> // provides sprintf() for formatting score text
#include <stdlib.h> // privates rand(),srand(), and abs()
#include <stdbool.h> //booleans
#include <time.h> // time() for seeding RNG
#include <SDL2/SDL.h> // main SDL2 library
#include <SDL2/SDL_image.h> // loads image files
#include <SDL2/SDL_mixer.h> // loads and plays sound effects
#include <SDL2/SDL_ttf.h> // renders TrueType fonts as text

// game window size in pixels
#define WINDOW_HEIGHT 600
#define WINDOW_WIDTH 800
// time between spawning food items
#define INTERVAL_ADD_MS 5000
// time before the hunter receives a new target food
#define INTERVAL_TARGET_MS 10000
// time between food movement updates
#define INTERVAL_MOVE_MS 32
// max amt of foods
#define MAX_FOOD 8

uint64_t last_tick_add;
uint64_t last_tick_move;
uint64_t last_tick_target;

// describes result of a food collision check
typedef enum eatflag
{
    WRONG,
    CORRECT,
    NOTHING
} EatFlag;

// stores the state of one moving food item
typedef struct food
{
    bool available;
    int x;
    int y;
    int dir_x;
    int dir_y;
    int food_index;
    SDL_Texture *texture;
} Food;

// stores the player position, sprite, and current target food
typedef struct hunter
{
    int x;
    int y;
    int target_food_index;
    SDL_Texture *texture_target_food;
    SDL_Texture *texture_hunter;
} Hunter;

// draws the current score in the top-left corner
void draw_score(SDL_Renderer *renderer, TTF_Font *font, int score)
{
    SDL_Color text_color = {255, 255, 255};

    char text[16];
    sprintf(text, "Score : %d", score);

    SDL_Surface *surface = TTF_RenderText_Solid(font, text, text_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int texW = 0, texH = 0;
    SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
    SDL_Rect dstrect = {0, 0, texW, texH};

    SDL_RenderCopy(renderer, texture, NULL, &dstrect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// clears the previous frame and draws the space background
void draw_background(SDL_Renderer *renderer, SDL_Texture *background)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = WINDOW_WIDTH;
    rect.h = WINDOW_HEIGHT;
    SDL_RenderCopy(renderer, background, NULL, &rect);
}

// draws the hunter and the small icon on the hunter's torso showing the target food
void draw_hunter(SDL_Renderer *renderer, Hunter hunter)
{
    SDL_Rect rect;
    rect.x = hunter.x;
    rect.y = hunter.y;
    rect.w = 100;
    rect.h = 100;
    SDL_RenderCopy(renderer, hunter.texture_hunter, NULL, &rect);

    SDL_Rect target_rect;
    target_rect.x = hunter.x + 37.5;
    target_rect.y = hunter.y + 50;
    target_rect.w = 25;
    target_rect.h = 25;
    SDL_RenderCopy(renderer, hunter.texture_target_food, NULL, &target_rect);
}

// draws every food item whose available flag is true
void draw_food(SDL_Renderer *renderer, Food *food_array)
{
    for (int i = 0; i < MAX_FOOD; i++)
    {
        if (food_array[i].available)
        {
            SDL_Rect rect;
            rect.x = food_array[i].x;
            rect.y = food_array[i].y;
            rect.w = 70;
            rect.h = 70;
            SDL_RenderCopy(renderer, food_array[i].texture, NULL, &rect);
        }
    }
}

/*
- checks whether the hunter is close enough to collect a food item
- returns CORRECT, WRONG or NOTHING.
*/
EatFlag hunter_eat_food(Hunter hunter, Food *food_array)
{
    for (int i = 0; i < MAX_FOOD; i++)
    {
        if (food_array[i].available)
        {
            int distance_from_mouth_x = abs((food_array[i].x + 35) - (hunter.x + 50));
            int distance_from_mouth_y = abs((food_array[i].y + 35) - (hunter.y + 50));

            if ((distance_from_mouth_x < 20) && (distance_from_mouth_y < 20))
            {
                food_array[i].available = false;

                if (food_array[i].food_index == hunter.target_food_index)
                {
                    return CORRECT;
                }
                else
                {
                    return WRONG;
                }
            }
        }
    }

    return NOTHING;
}

// randomly change the requested food after the target timer expires
void change_hunter_target(Hunter *hunter, SDL_Texture **texture_small_food)
{
    if (SDL_GetTicks() - last_tick_target > INTERVAL_TARGET_MS)
    {
        last_tick_target = SDL_GetTicks();
        (*hunter).target_food_index = rand() % 4;
        (*hunter).texture_target_food = texture_small_food[(*hunter).target_food_index];
    }
}

// briefly shows smoke where a food item changes direction
void flash_food_direction(SDL_Renderer *renderer, SDL_Texture *texture_smoke, int x, int y)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = 70;
    rect.h = 70;

    SDL_RenderCopy(renderer, texture_smoke, NULL, &rect);
    SDL_RenderPresent(renderer);
}

/*
more active food, wrap it around the window, randomly change directions,
and spawn a new food item at timed intervals*/
void add_or_move_food(Food *food_array, SDL_Texture **texture_food, int *food_index, SDL_Renderer *renderer, SDL_Texture *texture_smoke)
{
    // move food logic
    bool should_move_food = false;

    if (SDL_GetTicks() - last_tick_move > INTERVAL_MOVE_MS)
    {
        should_move_food = true;
        last_tick_move = SDL_GetTicks();
    }

    if (should_move_food)
    {
        for (int i = 0; i < MAX_FOOD; i++)
        {
            if (food_array[i].available)
            {
                if (food_array[i].x >= 0 && food_array[i].x <= WINDOW_WIDTH)
                {
                    food_array[i].x += food_array[i].dir_x;
                }
                else if (food_array[i].x > WINDOW_WIDTH)
                {
                    food_array[i].x = 0;
                }
                else
                {
                    food_array[i].x = WINDOW_WIDTH;
                }

                if (food_array[i].y >= 0 && food_array[i].y <= WINDOW_HEIGHT)
                {
                    food_array[i].y += food_array[i].dir_y;
                }
                else if (food_array[i].y > WINDOW_HEIGHT)
                {
                    food_array[i].y = 0;
                }
                else
                {
                    food_array[i].y = WINDOW_HEIGHT;
                }
            }
        }
    }

    // adds food logic
    bool should_add_food = false;

    if (SDL_GetTicks() - last_tick_add > INTERVAL_ADD_MS)
    {
        should_add_food = true;
        last_tick_add = SDL_GetTicks();
    }

    if (should_move_food)
    {
        for (int i = 0; i < MAX_FOOD; i++)
        {
            if (food_array[i].available)
            {
                // checks if the food should change direction
                if (rand() % 10 == 0)
                {
                    flash_food_direction(renderer, texture_smoke, food_array[i].x, food_array[i].y);

                    // changes direction randomly
                    food_array[i].dir_x = -5 + rand() % 10;
                    food_array[i].dir_y = -5 + rand() % 10;
                }
            }
        }
    }

    if (should_add_food)
    {
        Food new_food;

        new_food.available = true;
        new_food.food_index = rand() % 4;
        new_food.texture = texture_food[new_food.food_index];
        new_food.x = rand() % WINDOW_WIDTH;
        new_food.y = rand() % WINDOW_HEIGHT;

        new_food.dir_x = -5 + rand() % 10;
        new_food.dir_y = -5 + rand() % 10;

        food_array[*food_index] = new_food;

        if (*food_index < MAX_FOOD - 1)
        {
            (*food_index) += 1;
        }
        else
        {
            (*food_index) = 0;
        }
    }
}
// program entry point: initialise SDL, run the game loop, then clean up
int main(int argc, char **argv)
{   
    // use the current time so random behaviour differs each run
    srand(time(NULL));

    bool quit = false;
    SDL_Event event;

    // initializes SDL video, font and PNG-image support
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    TTF_Font *font = TTF_OpenFont("media/arial.ttf", 25);

    SDL_Window *window = SDL_CreateWindow("Food Hunter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    // loads both sound effects for both correct and wrong foods respectively
    Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 1024 );
    Mix_Chunk *audio_yuk = Mix_LoadWAV("media/Yuk.wav");
    Mix_Chunk *audio_yum = Mix_LoadWAV("media/Yum.wav");

    // loads image files into temporary SDL surfaces
    SDL_Surface *img_space = IMG_Load("media/space.png");
    SDL_Surface *img_hunter = IMG_Load("media/Hunter.png");
    SDL_Surface *img_burger = IMG_Load("media/Burger.png");
    SDL_Surface *img_chips = IMG_Load("media/Chips.png");
    SDL_Surface *img_icecream = IMG_Load("media/Icecream.png");
    SDL_Surface *img_pizza = IMG_Load("media/Pizza.png");
    SDL_Surface *img_small_burger = IMG_Load("media/SmallBurger.png");
    SDL_Surface *img_small_chips = IMG_Load("media/SmallChips.png");
    SDL_Surface *img_small_icecream = IMG_Load("media/SmallIcecream.png");
    SDL_Surface *img_small_pizza = IMG_Load("media/SmallPizza.png");

    // converts surfaces into GPU-friendly textures
    SDL_Texture *texture_space = SDL_CreateTextureFromSurface(renderer, img_space);
    SDL_Texture *texture_hunter = SDL_CreateTextureFromSurface(renderer, img_hunter);
    SDL_Texture *texture_food[4] = {
        SDL_CreateTextureFromSurface(renderer, img_burger),
        SDL_CreateTextureFromSurface(renderer, img_chips),
        SDL_CreateTextureFromSurface(renderer, img_icecream),
        SDL_CreateTextureFromSurface(renderer, img_pizza)};
    
    SDL_Texture *texture_small_food[4] = {
        SDL_CreateTextureFromSurface(renderer, img_small_burger),
        SDL_CreateTextureFromSurface(renderer, img_small_chips),
        SDL_CreateTextureFromSurface(renderer, img_small_icecream),
        SDL_CreateTextureFromSurface(renderer, img_small_pizza)};

    // clears sprite after creating texture
    SDL_FreeSurface(img_space);
    SDL_FreeSurface(img_hunter);
    SDL_FreeSurface(img_burger);
    SDL_FreeSurface(img_chips);
    SDL_FreeSurface(img_icecream);
    SDL_FreeSurface(img_pizza);
    SDL_FreeSurface(img_small_burger);
    SDL_FreeSurface(img_small_chips);
    SDL_FreeSurface(img_small_icecream);
    SDL_FreeSurface(img_small_pizza);

    // loads smoke surface
    SDL_Surface *img_smoke = IMG_Load("media/smoke.png");
    SDL_Texture *texture_smoke = SDL_CreateTextureFromSurface(renderer, img_smoke);
    SDL_FreeSurface(img_smoke);

    // creates the initial player, food array, and score state
    int food_renewal_idx = 0;
    Food food_array[MAX_FOOD] = {
        {.available = false},
        {.available = false},
        {.available = false},
        {.available = false},
        {.available = false},
        {.available = false},
        {.available = false},
        {.available = false}}; // limit to just MAX_FOOD food at anyone time

    Hunter hunter;
    hunter.texture_hunter = texture_hunter;
    hunter.texture_target_food = texture_small_food[0];
    hunter.x = 350;
    hunter.y = 250;

    int score = 0;

    /*
    Main game loop:
    1. Read input
    2. Update game logic
    3. Check collisions and scoring
    4. Draw the next frame
    */
    while (!quit)
    {
        // poll for event
        if (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                // arrow keys for character movement
                if (event.key.keysym.scancode == SDL_SCANCODE_LEFT)
                {
                    hunter.x -= 10;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT)
                {
                    hunter.x += 10;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_UP)
                {
                    hunter.y -= 10;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN)
                {
                    hunter.y += 10;
                }
                break;
            }
        }

        // updates hunter's requested food
        change_hunter_target(&hunter, texture_small_food);

        // checks whether food was collected and update the score accordingly
        switch(hunter_eat_food(hunter, food_array)){
            case WRONG:
                score -= 1;
                Mix_PlayChannel( -1, audio_yuk, 0 );
                break;
            case CORRECT:
                score += 1;
                Mix_PlayChannel( -1, audio_yum, 0 );
                break;
            default:
        }

        // draws background so other objects appear on top
        draw_background(renderer, texture_space);

        // draws every active food item
        draw_food(renderer, food_array);

        // draws player and target icon
        draw_hunter(renderer, hunter);

        // draws current score
        draw_score(renderer, font, score);

        // calls add_or_move_food after the renderer is declared
        add_or_move_food(food_array, texture_food, &food_renewal_idx, renderer, texture_smoke);

        // draw smoke effect for food changing direction
        for (int i = 0; i < MAX_FOOD; i++)
        {
            if (food_array[i].available && rand() % 10 == 0)
            {
                flash_food_direction(renderer, texture_smoke, food_array[i].x, food_array[i].y);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // does the screen cleanup
    SDL_DestroyTexture(texture_space);
    SDL_DestroyTexture(texture_hunter);

    // cleanup
    SDL_DestroyTexture(texture_smoke);
    for (int i = 0; i < 4; i++)
    {
        SDL_DestroyTexture(texture_food[i]);
        SDL_DestroyTexture(texture_small_food[i]);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_FreeChunk(audio_yuk);
    Mix_FreeChunk(audio_yum);

    TTF_CloseFont(font);

    TTF_Quit();
    IMG_Quit();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();

    return 0;
}

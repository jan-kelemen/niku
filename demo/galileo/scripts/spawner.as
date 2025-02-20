class spawner
{
    spawner(spawner_t@ obj)
    {
        @self = obj;
    }

    void on_character_contact()
    {
        if (self.should_spawn_sphere())
        {
            spawn_sphere();
        }
    }

    spawner_t@ self;
}

spawner@ create_spawner(spawner_t@ obj) {
    return spawner(obj);
}
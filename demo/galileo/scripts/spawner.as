class spawner
{
    spawner(spawner_data@ obj)
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

    spawner_data@ self;
}

spawner@ create_spawner(spawner_data@ obj) {
    return spawner(obj);
}
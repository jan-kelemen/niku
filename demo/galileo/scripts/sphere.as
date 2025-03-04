class sphere
{
    sphere(uint32 obj)
    {
        id = obj;
    }

    void on_hit(uint32 other)
    {
        if (is_spawner(other)) 
        {
            stop_pathfinding(id);
        }
    }

    uint32 id;
}

sphere@ create_sphere(uint32 obj) {
    return sphere(obj);
}
